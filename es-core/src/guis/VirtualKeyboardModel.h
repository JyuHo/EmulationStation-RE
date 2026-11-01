#pragma once

#include <string>
#include <vector>

// Layout, cursor, shift state and text of the on-screen keyboard (phase 11f5), without rendering:
// GuiVirtualKeyboard draws it and feeds it the input. Original layout of this project.
class VirtualKeyboardModel
{
public:
	enum class KeyKind
	{
		Char,
		Shift,
		Space,
		Backspace,
		Ok,
		Cancel
	};

	struct Key
	{
		KeyKind kind;
		std::string label; // lower case for the letters
		int start;		   // first column unit of the key, out of kColumns
		int width;		   // in column units
	};

	enum class Shift
	{
		Off,
		Once,  // the next letter only
		Locked // every letter until pressed again
	};

	enum class Result
	{
		None,
		Ok,
		Cancel
	};

	static constexpr int kColumns = 10;

	explicit VirtualKeyboardModel(std::string text = {});

	const std::vector<std::vector<Key>>& rows() const { return mRows; }
	int row() const { return mRow; }
	int column() const { return mKey; } // index of the selected key in its row
	const Key& selected() const { return mRows[mRow][mKey]; }
	Shift shift() const { return mShift; }
	const std::string& text() const { return mText; }

	// label shown on a key with the current shift state
	std::string displayLabel(const Key& key) const;

	// one step, wrapping around on each edge; vertical moves keep the column under the key center
	void move(int dx, int dy);
	// presses the selected key
	Result press();
	// removes the last character (UTF-8 aware); nothing on an empty text
	void backspace();

private:
	void type(const std::string& s);

	std::vector<std::vector<Key>> mRows;
	int mRow = 0;
	int mKey = 0;
	Shift mShift = Shift::Off;
	std::string mText;
};
