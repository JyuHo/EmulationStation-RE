#include "resources/TextureLoader.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <span>

namespace {
std::expected<ImageIO::Image, std::string> readAndDecode(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);
	if(!in)
		return std::unexpected(std::string("cannot open file"));
	const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return ImageIO::load(std::as_bytes(std::span(bytes)));
}

void eraseTicket(std::vector<TextureLoader::Ticket>& tickets, TextureLoader::Ticket ticket)
{
	std::erase(tickets, ticket);
}
} // namespace

TextureLoader::TextureLoader(unsigned int workerCount)
{
	workerCount = std::max(workerCount, 1u);
	mWorkers.reserve(workerCount);
	for(unsigned int i = 0; i < workerCount; i++)
		mWorkers.emplace_back([this](std::stop_token stop) { work(stop); });
}

TextureLoader::~TextureLoader()
{
	for(std::jthread& worker : mWorkers)
		worker.request_stop();
	mWake.notify_all();
	mWorkers.clear(); // joins
}

TextureLoader& TextureLoader::instance()
{
	static TextureLoader loader(std::clamp(std::thread::hardware_concurrency() / 2, 2u, 4u));
	return loader;
}

TextureLoader::Ticket TextureLoader::enqueue(std::string path)
{
	Ticket ticket = 0;
	{
		std::scoped_lock lock(mMutex);
		ticket = mNextTicket++;
		mQueue.push_back({ticket, std::move(path)});
	}
	mWake.notify_one();
	return ticket;
}

void TextureLoader::cancel(Ticket ticket)
{
	std::scoped_lock lock(mMutex);
	if(std::erase_if(mQueue, [ticket](const Job& job) { return job.ticket == ticket; }) > 0)
		return;
	if(std::ranges::find(mInProgress, ticket) != mInProgress.end())
	{
		mCancelled.push_back(ticket);
		return;
	}
	std::erase_if(mFinished, [ticket](const Result& result) { return result.ticket == ticket; });
}

std::vector<TextureLoader::Result> TextureLoader::takeFinished(std::size_t maxCount)
{
	std::scoped_lock lock(mMutex);
	const std::size_t count = std::min(maxCount, mFinished.size());
	std::vector<Result> taken(std::make_move_iterator(mFinished.begin()),
		std::make_move_iterator(mFinished.begin() + static_cast<std::ptrdiff_t>(count)));
	mFinished.erase(mFinished.begin(), mFinished.begin() + static_cast<std::ptrdiff_t>(count));
	return taken;
}

bool TextureLoader::idle() const
{
	std::scoped_lock lock(mMutex);
	return mQueue.empty() && mInProgress.empty() && mFinished.empty();
}

void TextureLoader::work(std::stop_token stop)
{
	while(true)
	{
		Job job{};
		{
			std::unique_lock lock(mMutex);
			if(!mWake.wait(lock, stop, [this] { return !mQueue.empty(); }))
				return; // stop requested
			job = std::move(mQueue.back());
			mQueue.pop_back();
			mInProgress.push_back(job.ticket);
		}

		Result result{job.ticket, job.path, readAndDecode(job.path)};

		std::scoped_lock lock(mMutex);
		eraseTicket(mInProgress, job.ticket);
		if(std::ranges::find(mCancelled, job.ticket) != mCancelled.end())
			eraseTicket(mCancelled, job.ticket);
		else
			mFinished.push_back(std::move(result));
	}
}
