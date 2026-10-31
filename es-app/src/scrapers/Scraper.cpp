#include "scrapers/Scraper.h"
#include "utils/Paths.h"
#include "Log.h"
#include "Settings.h"
#include "ImageIO.h"
#include <filesystem>
#include <fstream>

#include "GamesDBScraper.h"
#include "TheArchiveScraper.h"

const std::map<std::string, generate_scraper_requests_func> scraper_request_funcs = {
	{"TheGamesDB", &thegamesdb_generate_scraper_requests},
	{"TheArchive", &thearchive_generate_scraper_requests}};

std::unique_ptr<ScraperSearchHandle> startScraperSearch(const ScraperSearchParams& params)
{
	const std::string& name = Settings::getInstance()->getString("Scraper");

	std::unique_ptr<ScraperSearchHandle> handle = std::make_unique<ScraperSearchHandle>();
	scraper_request_funcs.at(name)(params, handle->mRequestQueue, handle->mResults);
	return handle;
}

std::vector<std::string> getScraperList()
{
	std::vector<std::string> list;
	for(auto it = scraper_request_funcs.begin(); it != scraper_request_funcs.end(); it++)
	{
		list.push_back(it->first);
	}

	return list;
}

// ScraperSearchHandle
ScraperSearchHandle::ScraperSearchHandle()
{
	setStatus(ASYNC_IN_PROGRESS);
}

void ScraperSearchHandle::update()
{
	if(mStatus == ASYNC_DONE)
		return;

	while(!mRequestQueue.empty())
	{
		auto& req = mRequestQueue.front();
		AsyncHandleStatus status = req->status();

		if(status == ASYNC_ERROR)
		{
			// propegate error
			setError(req->getStatusString());

			// empty our queue
			while(!mRequestQueue.empty())
				mRequestQueue.pop();

			return;
		}

		// finished this one, see if we have any more
		if(status == ASYNC_DONE)
		{
			mRequestQueue.pop();
			continue;
		}

		// status == ASYNC_IN_PROGRESS
	}

	// we finished without any errors!
	if(mRequestQueue.empty())
	{
		setStatus(ASYNC_DONE);
		return;
	}
}

// ScraperRequest
ScraperRequest::ScraperRequest(std::vector<ScraperSearchResult>& resultsWrite) : mResults(resultsWrite) {}

// ScraperHttpRequest
ScraperHttpRequest::ScraperHttpRequest(std::vector<ScraperSearchResult>& resultsWrite, const std::string& url) :
	ScraperRequest(resultsWrite)
{
	setStatus(ASYNC_IN_PROGRESS);
	mReq = std::make_unique<HttpReq>(url);
}

void ScraperHttpRequest::update()
{
	HttpReq::Status status = mReq->status();
	if(status == HttpReq::REQ_SUCCESS)
	{
		setStatus(ASYNC_DONE); // if process() has an error, status will be changed to ASYNC_ERROR
		process(mReq, mResults);
		return;
	}

	// not ready yet
	if(status == HttpReq::REQ_IN_PROGRESS)
		return;

	// everything else is some sort of error
	LOG(LogError) << "ScraperHttpRequest network error (status: " << status << ") - " << mReq->getErrorMsg();
	setError(mReq->getErrorMsg());
}

// metadata resolving stuff

std::unique_ptr<MDResolveHandle> resolveMetaDataAssets(
	const ScraperSearchResult& result, const ScraperSearchParams& search)
{
	return std::make_unique<MDResolveHandle>(result, search);
}

MDResolveHandle::MDResolveHandle(const ScraperSearchResult& result, const ScraperSearchParams& search) : mResult(result)
{
	if(!result.imageUrl.empty())
	{
		std::string imgPath = getSaveAsPath(search, "image", result.imageUrl);
		mFuncs.emplace_back(downloadImageAsync(result.imageUrl, imgPath), [this, imgPath] {
			mResult.mdl.set("image", imgPath);
			mResult.imageUrl = "";
		});
	}
}

void MDResolveHandle::update()
{
	if(mStatus == ASYNC_DONE || mStatus == ASYNC_ERROR)
		return;

	auto it = mFuncs.begin();
	while(it != mFuncs.end())
	{
		if(it->first->status() == ASYNC_ERROR)
		{
			setError(it->first->getStatusString());
			return;
		}
		else if(it->first->status() == ASYNC_DONE)
		{
			it->second();
			it = mFuncs.erase(it);
			continue;
		}
		it++;
	}

	if(mFuncs.empty())
		setStatus(ASYNC_DONE);
}

std::unique_ptr<ImageDownloadHandle> downloadImageAsync(const std::string& url, const std::string& saveAs)
{
	return std::make_unique<ImageDownloadHandle>(url, saveAs,
		Settings::getInstance()->getInt("ScraperResizeWidth"), Settings::getInstance()->getInt("ScraperResizeHeight"));
}

ImageDownloadHandle::ImageDownloadHandle(const std::string& url, const std::string& path, int maxWidth, int maxHeight) :
	mReq(std::make_unique<HttpReq>(url)), mSavePath(path), mMaxWidth(maxWidth), mMaxHeight(maxHeight)
{}

void ImageDownloadHandle::update()
{
	if(mReq->status() == HttpReq::REQ_IN_PROGRESS)
		return;

	if(mReq->status() != HttpReq::REQ_SUCCESS)
	{
		std::stringstream ss;
		ss << "Network error: " << mReq->getErrorMsg();
		setError(ss.str());
		return;
	}

	// download is done, save it to disk
	std::ofstream stream(mSavePath, std::ios_base::out | std::ios_base::binary);
	if(stream.bad())
	{
		setError("Failed to open image path to write. Permission error? Disk full?");
		return;
	}

	const std::string& content = mReq->getContent();
	stream.write(content.data(), content.length());
	stream.close();
	if(stream.bad())
	{
		setError("Failed to save image. Disk full?");
		return;
	}

	// resize it
	if(!resizeImage(mSavePath, mMaxWidth, mMaxHeight))
	{
		setError("Error saving resized image. Out of memory? Disk full?");
		return;
	}

	setStatus(ASYNC_DONE);
}

//you can pass 0 for width or height to keep aspect ratio
bool resizeImage(const std::string& path, int maxWidth, int maxHeight)
{
	return ImageIO::resizeImageFile(path, maxWidth, maxHeight);
}

std::string getSaveAsPath(const ScraperSearchParams& params, const std::string& suffix, const std::string& url)
{
	const std::string subdirectory = params.system->getName();
	const std::string name = params.game->getPath().stem().generic_string() + "-" + suffix;

	const std::filesystem::path dir = es::paths::userDir(es::paths::Kind::Data) / "downloaded_images" / subdirectory;
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);

	std::string path = dir.generic_string() + "/";

	size_t dot = url.find_last_of('.');
	std::string ext;
	if(dot != std::string::npos)
		ext = url.substr(dot, std::string::npos);

	path += name + ext;
	return path;
}
