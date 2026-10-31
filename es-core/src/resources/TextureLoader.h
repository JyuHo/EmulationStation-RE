#pragma once

#include "ImageIO.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Reads and decodes image files on worker threads. It never touches the renderer: the
// main thread collects the decoded pixels with takeFinished() and creates the textures
// (PROMPT.md 5.2: textures are created on the main thread only).
//
// The newest request is decoded first, so that scrolling quickly through a game list
// shows the selected game without waiting for the ones scrolled past; those are usually
// cancelled by then (their image component moved on).
class TextureLoader
{
public:
	using Ticket = std::uint64_t;

	struct Result
	{
		Ticket ticket = 0;
		std::string path;
		std::expected<ImageIO::Image, std::string> image;
	};

	explicit TextureLoader(unsigned int workerCount);
	~TextureLoader(); // stops the workers; a decode in progress is finished and dropped

	TextureLoader(const TextureLoader&) = delete;
	TextureLoader& operator=(const TextureLoader&) = delete;

	// The global loader, 2 to 4 workers depending on the CPU.
	static TextureLoader& instance();

	[[nodiscard]] Ticket enqueue(std::string path);
	// Forgets a request: dropped if still queued, its result discarded if being decoded.
	void cancel(Ticket ticket);
	// Results decoded since the last call, at most maxCount, oldest first.
	[[nodiscard]] std::vector<Result> takeFinished(std::size_t maxCount);
	// Nothing queued, being decoded or waiting to be taken.
	[[nodiscard]] bool idle() const;

private:
	struct Job
	{
		Ticket ticket;
		std::string path;
	};

	void work(std::stop_token stop);

	mutable std::mutex mMutex;
	std::condition_variable_any mWake;
	std::deque<Job> mQueue;
	std::vector<Ticket> mInProgress;
	std::vector<Ticket> mCancelled; // in progress when cancelled
	std::vector<Result> mFinished;
	Ticket mNextTicket = 1;
	std::vector<std::jthread> mWorkers; // last member: the threads stop before the rest is destroyed
};
