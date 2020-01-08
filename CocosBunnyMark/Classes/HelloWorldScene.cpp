/****************************************************************************
 Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#include "HelloWorldScene.h"

USING_NS_CC;


inline float rrand() { return (float)rand() / (float)RAND_MAX; }
inline float rrand(float a, float b) {
	float r = (float)rand() / (float)RAND_MAX;
	return a + r * (b - a);
}

const float bunniesAddingThreshold = 0.1f; // in seconds
const float gravity = 0.5f;

#if CC_TARGET_PLATFORM == CC_PLATFORM_WIN32
const uint32_t bunniesEachTime = 500;
#else
const uint32_t bunniesEachTime = 200;
#endif

Scene* HelloWorld::createScene()
{
    return HelloWorld::create();
}

bool HelloWorld::init()
{
    if (!Scene::init()) { return false; }

    Size visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin      = Director::getInstance()->getVisibleOrigin();

    texture = Director::getInstance()->getTextureCache()->addImage("bunnys.png");

    minX = origin.x;
    maxX = origin.x + visibleSize.width;
    minY = origin.y;
    maxY = origin.y + visibleSize.height;

    label = Label::createWithTTF("", "Roboto-Medium.ttf", 24);
    label->setTextColor(Color4B(255, 255, 255, 255));
    label->setAnchorPoint(Vec2(0, 1));
    label->setPosition(10, maxY - 10);
    addChild(label, 1);
    return true;
}

void HelloWorld::onEnter()
{
    Scene::onEnter();

    auto listener = EventListenerTouchOneByOne::create();
    listener->setSwallowTouches(true);

    listener->onTouchBegan = [this](Touch *touch, Event *event) {
		isTouchDown = true;
        return true;
    };

    listener->onTouchEnded = [this](Touch *touch, Event *event) {
		isTouchDown = false;
    };

    _eventDispatcher->addEventListenerWithFixedPriority(listener, 1);
    scheduleUpdate();
}

void HelloWorld::onExit()
{
    unscheduleUpdate();
    _eventDispatcher->removeEventListenersForTarget(this, false);

    bunnies.clear();
    Scene::onExit();
}

void HelloWorld::update(float dt)
{
    Node::update(dt);

	if (isTouchDown) {
		if (touchDownTime < 0) {
			touchDownTime = dt;
			addBunnies(bunniesEachTime);
		}
		else {
			touchDownTime += dt;
			if (touchDownTime > bunniesAddingThreshold) {
				// add bunnies!
				addBunnies(bunniesEachTime);
				touchDownTime -= bunniesAddingThreshold;
			}
		}
	}
	else if (touchDownTime > 0) {
		touchDownTime = -1.0f;
		currentTexId++;
		currentTexId %= 5;
	}

	float d = 60.f * dt; // pixijs's bunnymark work at 60 fps
	float gravityd = gravity * d;

	for (auto& bunny : bunnies) {
		bunny.x += bunny.speedX * d;
		bunny.y += bunny.speedY * d;
		bunny.speedY += gravityd;
		if (bunny.x > maxX) {
			bunny.speedX *= -1;
			bunny.x = maxX;
		}
		else if (bunny.x < 0) {
			bunny.speedX *= -1;
			bunny.x = 0;
		}
		if (bunny.y > maxY) {
			bunny.speedY *= -0.85f;
			bunny.y = maxY;
			if (rand() & 1) { // if (Math.rrand() > 0.5)
				bunny.speedY -= rrand() * 6;
			}
		}
		else if (bunny.y < 0) {
			bunny.speedY = 0;
			bunny.y = 0;
		}
		bunny.sprite->setPosition(bunny.x, maxY - bunny.y); // flip Y
	}
}

void HelloWorld::addBunnies(int amount)
{
    Bunny bunny;
    for (int i = 0; i < amount; i++) {
        initBunny(bunny);
        bunnies.push_back(bunny);
        addChild(bunny.sprite);
    }
    bunnyCount += amount;
    static char str[0x40];
    sprintf(str, "%d\nBUNNIES", bunnyCount);
    label->setString(str);
}

void HelloWorld::initBunny(Bunny& bunny)
{
	static const Rect textureRects[] = {
		Rect(2, 47, 26, 37),
		Rect(2, 86, 26, 37),
		Rect(2, 125, 26, 37),
		Rect(2, 164, 26, 37),
		Rect(2, 2, 26, 37),
	};
	bunny.sprite = Sprite::createWithTexture(texture, textureRects[currentTexId]);
	bunny.x = bunny.y = 0.f;
	bunny.speedX = rrand() * 10;
	bunny.speedY = rrand() * 10 - 5;
	bunny.sprite->setScale(rrand(0.5, 1.0));
	bunny.sprite->setRotation(CC_RADIANS_TO_DEGREES(rrand() - 0.5));
}
