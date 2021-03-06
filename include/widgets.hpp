#pragma once
#include <list>
#include <memory>

#include "../ext/nanovg/src/nanovg.h"
#include "../ext/oui-blendish/blendish.h"
#include "../ext/nanosvg/src/nanosvg.h"

#include "math.hpp"
#include "util.hpp"
#include "events.hpp"


#define SVG_DPI 75.0


namespace rack {


inline Vec in2px(Vec inches) {
	return inches.mult(SVG_DPI);
}

inline Vec mm2px(Vec millimeters) {
	return millimeters.mult(SVG_DPI / 25.4);
}


////////////////////
// resources
////////////////////

// Constructing these directly will load from the disk each time. Use the load() functions to load from disk and cache them as long as the shared_ptr is held.
// Implemented in gui.cpp

struct Font {
	int handle;
	Font(const std::string &filename);
	~Font();
	static std::shared_ptr<Font> load(const std::string &filename);
};

struct Image {
	int handle;
	Image(const std::string &filename);
	~Image();
	static std::shared_ptr<Image> load(const std::string &filename);
};

struct SVG {
	NSVGimage *handle;
	SVG(const std::string &filename);
	~SVG();
	static std::shared_ptr<SVG> load(const std::string &filename);
};


////////////////////
// Base widget
////////////////////

/** A node in the 2D scene graph */
struct Widget {
	/** Stores position and size */
	Rect box = Rect(Vec(), Vec(INFINITY, INFINITY));
	Widget *parent = NULL;
	std::list<Widget*> children;
	bool visible = true;

	virtual ~Widget();

	virtual Rect getChildrenBoundingBox();
	/**  Returns `v` transformed into the coordinate system of `relative` */
	virtual Vec getRelativeOffset(Vec v, Widget *relative);
	/** Returns `v` transformed into world coordinates */
	Vec getAbsoluteOffset(Vec v) {
		return getRelativeOffset(v, NULL);
	}
	/** Returns a subset of the given Rect bounded by the box of this widget and all ancestors */
	virtual Rect getViewport(Rect r);

	template <class T>
	T *getAncestorOfType() {
		if (!parent) return NULL;
		T *p = dynamic_cast<T*>(parent);
		if (p) return p;
		return parent->getAncestorOfType<T>();
	}

	template <class T>
	T *getFirstDescendantOfType() {
		for (Widget *child : children) {
			T *c = dynamic_cast<T*>(child);
			if (c) return c;
			c = child->getFirstDescendantOfType<T>();
			if (c) return c;
		}
		return NULL;
	}

	/** Adds widget to list of children.
	Gives ownership of widget to this widget instance.
	*/
	void addChild(Widget *widget);
	/** Removes widget from list of children if it exists.
	Does not delete widget but transfers ownership to caller
	Silently fails if widget is not a child
	*/
	void removeChild(Widget *widget);
	void clearChildren();
	/** Recursively finalizes event start/end pairs as needed */
	void finalizeEvents();

	/** Advances the module by one frame */
	virtual void step();
	/** Draws to NanoVG context */
	virtual void draw(NVGcontext *vg);

	// Events

	/** Called when a mouse button is pressed over this widget
	0 for left, 1 for right, 2 for middle.
	Return `this` to accept the event.
	Return NULL to reject the event and pass it to the widget behind this one.
	*/
	virtual void onMouseDown(EventMouseDown &e);
	virtual void onMouseUp(EventMouseUp &e);
	/** Called on every frame, even if mouseRel = Vec(0, 0) */
	virtual void onMouseMove(EventMouseMove &e);
	virtual void onHoverKey(EventHoverKey &e);
	/** Called when this widget begins responding to `onMouseMove` events */
	virtual void onMouseEnter(EventMouseEnter &e) {}
	/** Called when another widget begins responding to `onMouseMove` events */
	virtual void onMouseLeave(EventMouseLeave &e) {}
	virtual void onFocus(EventFocus &e) {}
	virtual void onDefocus(EventDefocus &e) {}
	virtual void onText(EventText &e) {}
	virtual void onKey(EventKey &e) {}
	virtual void onScroll(EventScroll &e);

	/** Called when a widget responds to `onMouseDown` for a left button press */
	virtual void onDragStart(EventDragStart &e) {}
	/** Called when the left button is released and this widget is being dragged */
	virtual void onDragEnd(EventDragEnd &e) {}
	/** Called when a widget responds to `onMouseMove` and is being dragged */
	virtual void onDragMove(EventDragMove &e) {}
	/** Called when a widget responds to `onMouseUp` for a left button release and a widget is being dragged */
	virtual void onDragEnter(EventDragEnter &e) {}
	virtual void onDragLeave(EventDragEnter &e) {}
	virtual void onDragDrop(EventDragDrop &e) {}
	virtual void onPathDrop(EventPathDrop &e);

	virtual void onAction(EventAction &e) {}
	virtual void onChange(EventChange &e) {}
	virtual void onZoom(EventZoom &e);
};

struct TransformWidget : Widget {
	/** The transformation matrix */
	float transform[6];
	TransformWidget();
	Rect getChildrenBoundingBox() override;
	void identity();
	void translate(Vec delta);
	void rotate(float angle);
	void scale(Vec s);
	void draw(NVGcontext *vg) override;
};

struct ZoomWidget : Widget {
	float zoom = 1.0;
	Vec getRelativeOffset(Vec v, Widget *relative) override;
	Rect getViewport(Rect r) override;
	void setZoom(float zoom);
	void draw(NVGcontext *vg) override;
	void onMouseDown(EventMouseDown &e) override;
	void onMouseUp(EventMouseUp &e) override;
	void onMouseMove(EventMouseMove &e) override;
	void onHoverKey(EventHoverKey &e) override;
	void onScroll(EventScroll &e) override;
};

////////////////////
// Trait widgets
////////////////////

/** Widget that does not respond to events */
struct TransparentWidget : virtual Widget {
	void onMouseDown(EventMouseDown &e) override {}
	void onMouseUp(EventMouseUp &e) override {}
	void onMouseMove(EventMouseMove &e) override {}
	void onScroll(EventScroll &e) override {}
};

/** Widget that automatically responds to all mouse events but gives a chance for children to respond instead */
struct OpaqueWidget : virtual Widget {
	void onMouseDown(EventMouseDown &e) override {
		Widget::onMouseDown(e);
		if (!e.target)
			e.target = this;
		e.consumed = true;
	}
	void onMouseUp(EventMouseUp &e) override {
		Widget::onMouseUp(e);
		if (!e.target)
			e.target = this;
		e.consumed = true;
	}
	void onMouseMove(EventMouseMove &e) override {
		Widget::onMouseMove(e);
		if (!e.target)
			e.target = this;
		e.consumed = true;
	}
	void onScroll(EventScroll &e) override {
		Widget::onScroll(e);
		e.consumed = true;
	}
};

struct SpriteWidget : virtual Widget {
	Vec spriteOffset;
	Vec spriteSize;
	std::shared_ptr<Image> spriteImage;
	int index = 0;
	void draw(NVGcontext *vg) override;
};

struct SVGWidget : virtual Widget {
	std::shared_ptr<SVG> svg;
	/** Sets the box size to the svg image size */
	void wrap();
	/** Sets and wraps the SVG */
	void setSVG(std::shared_ptr<SVG> svg);
	void draw(NVGcontext *vg) override;
};

/** Caches a widget's draw() result to a framebuffer so it is called less frequently
When `dirty` is true, its children will be re-rendered on the next call to step() override.
Events are not passed to the underlying scene.
*/
struct FramebufferWidget : virtual Widget {
	/** Set this to true to re-render the children to the framebuffer the next time it is drawn */
	bool dirty = true;
	/** A margin in pixels around the children in the framebuffer
	This prevents cutting the rendered SVG off on the box edges.
	*/
	float oversample;
	/** The root object in the framebuffer scene
	The FramebufferWidget owns the pointer
	*/
	struct Internal;
	Internal *internal;

	FramebufferWidget();
	~FramebufferWidget();
	void draw(NVGcontext *vg) override;
	int getImageHandle();
	void onZoom(EventZoom &e) override;
};

struct QuantityWidget : virtual Widget {
	float value = 0.0;
	float minValue = 0.0;
	float maxValue = 1.0;
	float defaultValue = 0.0;
	std::string label;
	/** Include a space character if you want a space after the number, e.g. " Hz" */
	std::string unit;
	/** The decimal place to round for displaying values.
	A precision of 2 will display as "1.00" for example.
	*/
	int precision = 2;

	QuantityWidget();
	void setValue(float value);
	void setLimits(float minValue, float maxValue);
	void setDefaultValue(float defaultValue);
	/** Generates the display value */
	std::string getText();
};

////////////////////
// GUI widgets
////////////////////

struct Label : Widget {
	std::string text;
	Label() {
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(NVGcontext *vg) override;
};

/** Deletes itself from parent when clicked */
struct MenuOverlay : OpaqueWidget {
	void step() override;
	void onMouseDown(EventMouseDown &e) override;
	void onHoverKey(EventHoverKey &e) override;
};

struct MenuEntry;

struct Menu : OpaqueWidget {
	Menu *parentMenu = NULL;
	Menu *childMenu = NULL;
	/** The entry which created the child menu */
	MenuEntry *activeEntry = NULL;

	Menu() {
		box.size = Vec(0, 0);
	}
	~Menu();
	// Resizes menu and calls addChild()
	void pushChild(Widget *child) DEPRECATED {
		addChild(child);
	}
	void setChildMenu(Menu *menu);
	void step() override;
	void draw(NVGcontext *vg) override;
	void onScroll(EventScroll &e) override;
};

struct MenuEntry : OpaqueWidget {
	std::string text;
	MenuEntry() {
		box.size = Vec(0, BND_WIDGET_HEIGHT);
	}
};

struct MenuLabel : MenuEntry {
	void draw(NVGcontext *vg) override;
	void step() override;
};

struct MenuItem : MenuEntry {
	std::string rightText;
	void draw(NVGcontext *vg) override;
	void step() override;
	virtual Menu *createChildMenu() {return NULL;}
	void onMouseEnter(EventMouseEnter &e) override;
	void onDragDrop(EventDragDrop &e) override;
};

struct WindowOverlay : OpaqueWidget {
};

struct Window : OpaqueWidget {
	std::string title;
	void draw(NVGcontext *vg) override;
	void onDragMove(EventDragMove &e) override;
};

struct Button : OpaqueWidget {
	std::string text;
	BNDwidgetState state = BND_DEFAULT;

	Button() {
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(NVGcontext *vg) override;
	void onMouseEnter(EventMouseEnter &e) override;
	void onMouseLeave(EventMouseLeave &e) override;
	void onDragStart(EventDragStart &e) override;
	void onDragEnd(EventDragEnd &e) override;
	void onDragDrop(EventDragDrop &e) override;
};

struct ChoiceButton : Button {
	void draw(NVGcontext *vg) override;
};

struct RadioButton : OpaqueWidget, QuantityWidget {
	BNDwidgetState state = BND_DEFAULT;

	RadioButton() {
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(NVGcontext *vg) override;
	void onMouseEnter(EventMouseEnter &e) override;
	void onMouseLeave(EventMouseLeave &e) override;
	void onDragDrop(EventDragDrop &e) override;
};

struct Slider : OpaqueWidget, QuantityWidget {
	BNDwidgetState state = BND_DEFAULT;

	Slider() {
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(NVGcontext *vg) override;
	void onDragStart(EventDragStart &e) override;
	void onDragMove(EventDragMove &e) override;
	void onDragEnd(EventDragEnd &e) override;
	void onMouseDown(EventMouseDown &e) override;
};

/** Parent must be a ScrollWidget */
struct ScrollBar : OpaqueWidget {
	enum { VERTICAL, HORIZONTAL } orientation;
	BNDwidgetState state = BND_DEFAULT;
	float offset = 0.0;
	float size = 0.0;

	ScrollBar() {
		box.size = Vec(BND_SCROLLBAR_WIDTH, BND_SCROLLBAR_HEIGHT);
	}
	void draw(NVGcontext *vg) override;
	void onDragStart(EventDragStart &e) override;
	void onDragMove(EventDragMove &e) override;
	void onDragEnd(EventDragEnd &e) override;
};

/** Handles a container with ScrollBar */
struct ScrollWidget : OpaqueWidget {
	Widget *container;
	ScrollBar *horizontalScrollBar;
	ScrollBar *verticalScrollBar;
	Vec offset;

	ScrollWidget();
	void draw(NVGcontext *vg) override;
	void step() override;
	void onMouseMove(EventMouseMove &e) override;
	void onScroll(EventScroll &e) override;
};

struct TextField : OpaqueWidget {
	std::string text;
	std::string placeholder;
	bool multiline = false;
	int begin = 0;
	int end = 0;

	TextField() {
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(NVGcontext *vg) override;
	void onMouseDown(EventMouseDown &e) override;
	void onFocus(EventFocus &e) override;
	void onText(EventText &e) override;
	void onKey(EventKey &e) override;
	void insertText(std::string newText);
	virtual void onTextChange() {}
};

struct PasswordField : TextField {
	void draw(NVGcontext *vg) override;
};

struct ProgressBar : TransparentWidget, QuantityWidget {
	ProgressBar() {
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(NVGcontext *vg) override;
};

struct Tooltip : Widget {
	void step() override;
	void draw(NVGcontext *vg) override;
};

struct Scene : OpaqueWidget {
	Widget *overlay = NULL;
	void setOverlay(Widget *w);
	Menu *createMenu();
	void step() override;
};


////////////////////
// globals
////////////////////

extern Widget *gHoveredWidget;
extern Widget *gDraggedWidget;
extern Widget *gDragHoveredWidget;
extern Widget *gFocusedWidget;

extern Scene *gScene;


} // namespace rack
