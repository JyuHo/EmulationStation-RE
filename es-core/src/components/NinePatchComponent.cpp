#include "components/NinePatchComponent.h"
#include "Window.h"
#include "Log.h"
#include "renderer/Renderer.h"
#include "ThemeData.h"
#include "Util.h"

NinePatchComponent::NinePatchComponent(
	Window* window, const std::string& path, unsigned int edgeColor, unsigned int centerColor) :
	GuiComponent(window),
	mVertices(nullptr),
	mColors(nullptr),
	mPath(path),
	mEdgeColor(edgeColor),
	mCenterColor(centerColor)
{
	if(!mPath.empty())
		buildVertices();
}

NinePatchComponent::~NinePatchComponent() = default;

void NinePatchComponent::updateColors()
{
	Renderer::buildGLColorArray(mColors.get(), mEdgeColor, 6 * 9);
	Renderer::buildGLColorArray(mColors.get() + 4 * 6 * 4, mCenterColor, 6);
}

void NinePatchComponent::buildVertices()
{
	mVertices.reset();
	mColors.reset();

	mTexture = TextureResource::get(mPath);

	if(mTexture->getSize() == glm::ivec2(0))
	{
		mVertices = nullptr;
		mColors = nullptr;
		LOG(LogWarning) << "NinePatchComponent missing texture!";
		return;
	}

	mVertices = std::make_unique<Vertex[]>(6 * 9);
	mColors = std::make_unique<GLubyte[]>(6 * 9 * 4);
	updateColors();

	const glm::vec2 ts = glm::vec2(mTexture->getSize());

	//coordinates on the image in pixels, top left origin
	const glm::vec2 pieceCoords[9] = {
		glm::vec2(0, 0),
		glm::vec2(16, 0),
		glm::vec2(32, 0),
		glm::vec2(0, 16),
		glm::vec2(16, 16),
		glm::vec2(32, 16),
		glm::vec2(0, 32),
		glm::vec2(16, 32),
		glm::vec2(32, 32),
	};

	const glm::vec2 pieceSizes = getCornerSize();

	//corners never stretch, so we calculate a width and height for slices 1, 3, 5, and 7
	float borderWidth = mSize.x - (pieceSizes.x * 2); //should be pieceSizes[0] and pieceSizes[2]
	//if(borderWidth < pieceSizes.x)
	//	borderWidth = pieceSizes.x;

	float borderHeight = mSize.y - (pieceSizes.y * 2); //should be pieceSizes[0] and pieceSizes[6]
	//if(borderHeight < pieceSizes.y)
	//	borderHeight = pieceSizes.y;

	mVertices[0 * 6].pos = pieceCoords[0];									 //top left
	mVertices[1 * 6].pos = pieceCoords[1];									 //top middle
	mVertices[2 * 6].pos = pieceCoords[1] + glm::vec2(borderWidth, 0);		 //top right

	mVertices[3 * 6].pos = mVertices[0 * 6].pos + glm::vec2(0, pieceSizes.y); //mid left
	mVertices[4 * 6].pos = mVertices[3 * 6].pos + glm::vec2(pieceSizes.x, 0); //mid middle
	mVertices[5 * 6].pos = mVertices[4 * 6].pos + glm::vec2(borderWidth, 0);  //mid right

	mVertices[6 * 6].pos = mVertices[3 * 6].pos + glm::vec2(0, borderHeight); //bot left
	mVertices[7 * 6].pos = mVertices[6 * 6].pos + glm::vec2(pieceSizes.x, 0); //bot middle
	mVertices[8 * 6].pos = mVertices[7 * 6].pos + glm::vec2(borderWidth, 0);  //bot right

	int v = 0;
	for(int slice = 0; slice < 9; slice++)
	{
		glm::vec2 size;

		//corners
		if(slice == 0 || slice == 2 || slice == 6 || slice == 8)
			size = pieceSizes;

		//vertical borders
		if(slice == 1 || slice == 7)
			size = {borderWidth, pieceSizes.y};

		//horizontal borders
		if(slice == 3 || slice == 5)
			size = {pieceSizes.x, borderHeight};

		//center
		if(slice == 4)
			size = {borderWidth, borderHeight};

		//no resizing will be necessary
		//mVertices[v + 0] is already correct
		mVertices[v + 1].pos = mVertices[v + 0].pos + size;
		mVertices[v + 2].pos = {mVertices[v + 0].pos.x, mVertices[v + 1].pos.y};

		mVertices[v + 3].pos = {mVertices[v + 1].pos.x, mVertices[v + 0].pos.y};
		mVertices[v + 4].pos = mVertices[v + 1].pos;
		mVertices[v + 5].pos = mVertices[v + 0].pos;

		//texture coordinates
		//the y = (1 - y) is to deal with texture coordinates having a bottom left corner origin vs. verticies having a top left origin
		mVertices[v + 0].tex = {pieceCoords[slice].x / ts.x, 1 - (pieceCoords[slice].y / ts.y)};
		mVertices[v + 1].tex = {
			(pieceCoords[slice].x + pieceSizes.x) / ts.x, 1 - ((pieceCoords[slice].y + pieceSizes.y) / ts.y)};
		mVertices[v + 2].tex = {mVertices[v + 0].tex.x, mVertices[v + 1].tex.y};

		mVertices[v + 3].tex = {mVertices[v + 1].tex.x, mVertices[v + 0].tex.y};
		mVertices[v + 4].tex = mVertices[v + 1].tex;
		mVertices[v + 5].tex = mVertices[v + 0].tex;

		v += 6;
	}

	// round vertices
	for(int i = 0; i < 6 * 9; i++)
	{
		mVertices[i].pos = roundVector(mVertices[i].pos);
	}
}

void NinePatchComponent::render(const glm::mat4& parentTrans)
{
	glm::mat4 trans = roundMatrix(parentTrans * getTransform());

	if(mTexture && mVertices != nullptr)
	{
		Renderer::setMatrix(trans);

		mTexture->bind();

		std::vector<Renderer::Vertex> verts(6 * 9);
		for(int i = 0; i < 6 * 9; i++)
		{
			verts[i].pos = mVertices[i].pos;
			verts[i].tex = mVertices[i].tex;
			verts[i].color = ((unsigned int)mColors[i * 4]) << 24 | ((unsigned int)mColors[i * 4 + 1]) << 16 |
				((unsigned int)mColors[i * 4 + 2]) << 8 | ((unsigned int)mColors[i * 4 + 3]);
		}
		Renderer::drawTriangles(std::span<const Renderer::Vertex>(verts), mTexture->getTextureID(), GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	renderChildren(trans);
}

void NinePatchComponent::onSizeChanged()
{
	buildVertices();
}

glm::vec2 NinePatchComponent::getCornerSize() const
{
	return glm::vec2(16, 16);
}

void NinePatchComponent::fitTo(glm::vec2 size, glm::vec3 position, glm::vec2 padding)
{
	size += padding;
	position[0] -= padding.x / 2;
	position[1] -= padding.y / 2;

	setSize(size + glm::vec2(getCornerSize().x * 2, getCornerSize().y * 2));
	setPosition(-getCornerSize().x + position.x, -getCornerSize().y + position.y);
}

void NinePatchComponent::setImagePath(const std::string& path)
{
	mPath = path;
	buildVertices();
}

void NinePatchComponent::setEdgeColor(unsigned int edgeColor)
{
	mEdgeColor = edgeColor;
	updateColors();
}

void NinePatchComponent::setCenterColor(unsigned int centerColor)
{
	mCenterColor = centerColor;
	updateColors();
}

void NinePatchComponent::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view,
	const std::string& element, unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties);

	using namespace ThemeFlags;

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "ninepatch");
	if(!elem)
		return;

	if(properties & PATH && elem->has("path"))
		setImagePath(elem->get<std::string>("path"));
}
