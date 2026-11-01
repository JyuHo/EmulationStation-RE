#include "guis/VirtualKeyboardModel.h"

namespace {

std::vector<VirtualKeyboardModel::Key> charRow(const char* chars)
{
	std::vector<VirtualKeyboardModel::Key> row;
	for(int i = 0; chars[i]; i++)
		row.push_back({VirtualKeyboardModel::KeyKind::Char, std::string(1, chars[i]), i, 1});
	return row;
}

} // namespace

VirtualKeyboardModel::VirtualKeyboardModel(std::string text) : mText(std::move(text))
{
	using K = KeyKind;
	mRows.push_back(charRow("1234567890"));
	mRows.push_back(charRow("qwertyuiop"));
	mRows.push_back(charRow("asdfghjkl'"));
	mRows.push_back(charRow("zxcvbnm-_."));
	// labels of the special keys are set by GuiVirtualKeyboard (translated)
	mRows.push_back({{K::Shift, "", 0, 2}, {K::Space, "", 2, 2}, {K::Backspace, "", 4, 2}, {K::Ok, "", 6, 2},
		{K::Cancel, "", 8, 2}});
}

std::string VirtualKeyboardModel::displayLabel(const Key& key) const
{
	if(key.kind != KeyKind::Char || mShift == Shift::Off)
		return key.label;
	std::string s = key.label;
	for(char& c : s)
		if(c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
	return s;
}

void VirtualKeyboardModel::move(int dx, int dy)
{
	const int rowCount = (int)mRows.size();
	if(dx != 0)
	{
		const int n = (int)mRows[mRow].size();
		mKey = ((mKey + dx) % n + n) % n;
	}
	if(dy != 0)
	{
		const Key& from = mRows[mRow][mKey];
		const int center2 = from.start * 2 + from.width; // twice the center, in column units
		mRow = ((mRow + dy) % rowCount + rowCount) % rowCount;
		const auto& to = mRows[mRow];
		mKey = (int)to.size() - 1;
		for(int i = 0; i < (int)to.size(); i++)
			if(center2 <= (to[i].start + to[i].width) * 2) // a center on a boundary: the left key
			{
				mKey = i;
				break;
			}
	}
}

void VirtualKeyboardModel::type(const std::string& s)
{
	mText += s;
}

VirtualKeyboardModel::Result VirtualKeyboardModel::press()
{
	const Key& key = selected();
	switch(key.kind)
	{
		case KeyKind::Char:
			type(displayLabel(key));
			if(mShift == Shift::Once)
				mShift = Shift::Off;
			break;
		case KeyKind::Shift:
			mShift = mShift == Shift::Off ? Shift::Once : mShift == Shift::Once ? Shift::Locked : Shift::Off;
			break;
		case KeyKind::Space:
			type(" ");
			break;
		case KeyKind::Backspace:
			backspace();
			break;
		case KeyKind::Ok:
			return Result::Ok;
		case KeyKind::Cancel:
			return Result::Cancel;
	}
	return Result::None;
}

void VirtualKeyboardModel::backspace()
{
	if(mText.empty())
		return;
	// drop the continuation bytes (10xxxxxx), then the lead byte
	size_t end = mText.size() - 1;
	while(end > 0 && ((unsigned char)mText[end] & 0xC0) == 0x80)
		end--;
	mText.erase(end);
}
