/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2011      Zynga Inc.

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

Use any of these editors to generate BMFonts:
http://glyphdesigner.71squared.com/ (Commercial, Mac OS X)
http://www.n4te.com/hiero/hiero.jnlp (Free, Java)
http://slick.cokeandcode.com/demos/hiero.jnlp (Free, Java)
http://www.angelcode.com/products/bmfont/ (Free, Windows only)

****************************************************************************/
//#define mobileShaderDebug
#define mobileShaderCompileOnce

#include "LabelBMFont.h"

#include "cocoa/CCString.h"
#include "platform/platform.h"
#include "cocoa/CCDictionary.h"
#include "CCConfiguration.h"
#include "draw_nodes/CCDrawingPrimitives.h"
#include "sprite_nodes/CCSprite.h"
#include "support/CCPointExtension.h"
#include "platform/CCFileUtils.h"
#include "CCDirector.h"
#include "textures/CCTextureCache.h"
#include "support/ccUTF8.h"
#include "shaders/CCShaderCache.h"
#include "actions/CCActionInterval.h"
/*
#include "cocos2d.h"
*/
using namespace std;

NS_BMF_BEGIN
USING_NS_CC;
// The return value needs to be deleted by CC_SAFE_DELETE_ARRAY.
static unsigned short* copyUTF16StringN(unsigned short* str)
{
    int length = str ? cc_wcslen(str) : 0;
    unsigned short* ret = new unsigned short[length+1];
    for (int i = 0; i < length; ++i) {
        ret[i] = str[i];
    }
    ret[length] = 0;
    return ret;
}

//
//FNTConfig Cache - free functions
//
static CCDictionary* s_pConfigurations = NULL;

BMFontConfiguration* FNTConfigLoadFile_ex( const char *fntFile)
{
    BMFontConfiguration* pRet = NULL;

    if( s_pConfigurations == NULL )
    {
        s_pConfigurations = new CCDictionary();
    }

    pRet = (BMFontConfiguration*)s_pConfigurations->objectForKey(fntFile);
    if( pRet == NULL )
    {
        pRet = BMFontConfiguration::create(fntFile);
        if (pRet)
        {
            s_pConfigurations->setObject(pRet, fntFile);
        }        
    }

    return pRet;
}

void FNTConfigRemoveCache_ex( void )
{
    if (s_pConfigurations)
    {
        s_pConfigurations->removeAllObjects();
        CC_SAFE_RELEASE_NULL(s_pConfigurations);
    }
}

//
//BitmapFontConfiguration
//

BMFontConfiguration * BMFontConfiguration::create(const char *FNTfile)
{
    BMFontConfiguration * pRet = new BMFontConfiguration();
    if (pRet->initWithFNTfile(FNTfile))
    {
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return NULL;
}

bool BMFontConfiguration::initWithFNTfile(const char *FNTfile)
{
    m_pKerningDictionary = NULL;
    m_pFontDefDictionary = NULL;
    
    m_pCharacterSet = this->parseConfigFile(FNTfile);
    
    if (! m_pCharacterSet)
    {
        return false;
    }

    return true;
}

std::set<unsigned int>* BMFontConfiguration::getCharacterSet() const
{
    return m_pCharacterSet;
}

BMFontConfiguration::BMFontConfiguration()
: m_pFontDefDictionary(NULL)
, m_nCommonHeight(0)
, m_pKerningDictionary(NULL)
, m_pCharacterSet(NULL)
{

}

BMFontConfiguration::~BMFontConfiguration()
{
    CCLOGINFO( "cocos2d: deallocing BMFontConfiguration" );
    this->purgeFontDefDictionary();
    this->purgeKerningDictionary();
    m_sAtlasName.clear();
    CC_SAFE_DELETE(m_pCharacterSet);
}

const char* BMFontConfiguration::description(void)
{
    return CCString::createWithFormat(
        "<BMFontConfiguration = " CC_FORMAT_PRINTF_SIZE_T " | Glphys:%d Kernings:%d | Image = %s>",
        (size_t)this,
        HASH_COUNT(m_pFontDefDictionary),
        HASH_COUNT(m_pKerningDictionary),
        m_sAtlasName.c_str()
    )->getCString();
}

void BMFontConfiguration::purgeKerningDictionary()
{
    KerningHashElement *current;
    while(m_pKerningDictionary) 
    {
        current = m_pKerningDictionary; 
        HASH_DEL(m_pKerningDictionary,current);
        free(current);
    }
}

void BMFontConfiguration::purgeFontDefDictionary()
{    
    FontDefHashElement *current, *tmp;

    HASH_ITER(hh, m_pFontDefDictionary, current, tmp) {
        HASH_DEL(m_pFontDefDictionary, current);
        free(current);
    }
}

std::set<unsigned int>* BMFontConfiguration::parseConfigFile(const char *controlFile)
{    
    std::string fullpath = CCFileUtils::sharedFileUtils()->fullPathForFilename(controlFile);
    CCString *contents = CCString::createWithContentsOfFile(fullpath.c_str());

    CCAssert(contents, "BMFontConfiguration::parseConfigFile | Open file error.");
    
    set<unsigned int> *validCharsString = new set<unsigned int>();

    if (!contents)
    {
        CCLOG("cocos2d: Error parsing FNTfile %s", controlFile);
        return NULL;
    }

    // parse spacing / padding
    std::string line;
    std::string strLeft = contents->getCString();
    while (strLeft.length() > 0)
    {
        int pos = strLeft.find('\n');

        if (pos != (int)std::string::npos)
        {
            // the data is more than a line.get one line
            line = strLeft.substr(0, pos);
            strLeft = strLeft.substr(pos + 1);
        }
        else
        {
            // get the left data
            line = strLeft;
            strLeft.erase();
        }

        if(line.substr(0,strlen("info face")) == "info face") 
        {
            // XXX: info parsing is incomplete
            // Not needed for the Hiero editors, but needed for the AngelCode editor
            //            [self parseInfoArguments:line];
            this->parseInfoArguments(line);
        }
        // Check to see if the start of the line is something we are interested in
        else if(line.substr(0,strlen("common lineHeight")) == "common lineHeight")
        {
            this->parseCommonArguments(line);
        }
        else if(line.substr(0,strlen("page id")) == "page id")
        {
            this->parseImageFileName(line, controlFile);
        }
        else if(line.substr(0,strlen("chars c")) == "chars c")
        {
            // Ignore this line
        }
        else if(line.substr(0,strlen("char")) == "char")
        {
            // Parse the current line and create a new CharDef
            FontDefHashElement* element = (FontDefHashElement*)malloc( sizeof(*element) );
            this->parseCharacterDefinition(line, &element->fontDef);

            element->key = element->fontDef.charID;
            HASH_ADD_INT(m_pFontDefDictionary, key, element);
            
            validCharsString->insert(element->fontDef.charID);
        }
//        else if(line.substr(0,strlen("kernings count")) == "kernings count")
//        {
//            this->parseKerningCapacity(line);
//        }
        else if(line.substr(0,strlen("kerning first")) == "kerning first")
        {
            this->parseKerningEntry(line);
        }
    }
    
    return validCharsString;
}

void BMFontConfiguration::parseImageFileName(std::string line, const char *fntFile)
{
    //////////////////////////////////////////////////////////////////////////
    // line to parse:
    // page id=0 file="bitmapFontTest.png"
    //////////////////////////////////////////////////////////////////////////

    // page ID. Sanity check
    int index = line.find('=')+1;
    int index2 = line.find(' ', index);
    std::string value = line.substr(index, index2-index);
    CCAssert(atoi(value.c_str()) == 0, "LabelBMFont file could not be found");
    // file 
    index = line.find('"')+1;
    index2 = line.find('"', index);
    value = line.substr(index, index2-index);

    m_sAtlasName = CCFileUtils::sharedFileUtils()->fullPathFromRelativeFile(value.c_str(), fntFile);
}

void BMFontConfiguration::parseInfoArguments(std::string line)
{
    //////////////////////////////////////////////////////////////////////////
    // possible lines to parse:
    // info face="Script" size=32 bold=0 italic=0 charset="" unicode=1 stretchH=100 smooth=1 aa=1 padding=1,4,3,2 spacing=0,0 outline=0
    // info face="Cracked" size=36 bold=0 italic=0 charset="" unicode=0 stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=1,1
    //////////////////////////////////////////////////////////////////////////

    // padding
    int index = line.find("padding=");
    int index2 = line.find(' ', index);
    std::string value = line.substr(index, index2-index);
    sscanf(value.c_str(), "padding=%d,%d,%d,%d", &m_tPadding.top, &m_tPadding.right, &m_tPadding.bottom, &m_tPadding.left);
    CCLOG("cocos2d: padding: %d,%d,%d,%d", m_tPadding.left, m_tPadding.top, m_tPadding.right, m_tPadding.bottom);
}

void BMFontConfiguration::parseCommonArguments(std::string line)
{
    //////////////////////////////////////////////////////////////////////////
    // line to parse:
    // common lineHeight=104 base=26 scaleW=1024 scaleH=512 pages=1 packed=0
    //////////////////////////////////////////////////////////////////////////

    // Height
    int index = line.find("lineHeight=");
    int index2 = line.find(' ', index);
    std::string value = line.substr(index, index2-index);
    sscanf(value.c_str(), "lineHeight=%d", &m_nCommonHeight);
    // scaleW. sanity check
    index = line.find("scaleW=") + strlen("scaleW=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    CCAssert(atoi(value.c_str()) <= CCConfiguration::sharedConfiguration()->getMaxTextureSize(), "LabelBMFont: page can't be larger than supported");
    // scaleH. sanity check
    index = line.find("scaleH=") + strlen("scaleH=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    CCAssert(atoi(value.c_str()) <= CCConfiguration::sharedConfiguration()->getMaxTextureSize(), "LabelBMFont: page can't be larger than supported");
    // pages. sanity check
    index = line.find("pages=") + strlen("pages=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    CCAssert(atoi(value.c_str()) == 1, "CCBitfontAtlas: only supports 1 page");

    // packed (ignore) What does this mean ??
}

void BMFontConfiguration::parseCharacterDefinition(std::string line, BMFontDef *characterDefinition)
{    
    //////////////////////////////////////////////////////////////////////////
    // line to parse:
    // char id=32   x=0     y=0     width=0     height=0     xoffset=0     yoffset=44    xadvance=14     page=0  chnl=0 
    //////////////////////////////////////////////////////////////////////////

    // Character ID
    int index = line.find("id=");
    int index2 = line.find(' ', index);
    std::string value = line.substr(index, index2-index);
    sscanf(value.c_str(), "id=%u", &characterDefinition->charID);

    // Character x
    index = line.find("x=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "x=%f", &characterDefinition->rect.origin.x);
    // Character y
    index = line.find("y=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "y=%f", &characterDefinition->rect.origin.y);
    // Character width
    index = line.find("width=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "width=%f", &characterDefinition->rect.size.width);
    // Character height
    index = line.find("height=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "height=%f", &characterDefinition->rect.size.height);
    // Character xoffset
    index = line.find("xoffset=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "xoffset=%hd", &characterDefinition->xOffset);
    // Character yoffset
    index = line.find("yoffset=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "yoffset=%hd", &characterDefinition->yOffset);
    // Character xadvance
    index = line.find("xadvance=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "xadvance=%hd", &characterDefinition->xAdvance);
	// Character chnl
	index = line.find("chnl=");
	index2 = line.find(' ', index);
	value = line.substr(index, index2-index);
	sscanf(value.c_str(), "chnl=%hd", &characterDefinition->chnl);
}

void BMFontConfiguration::parseKerningEntry(std::string line)
{        
    //////////////////////////////////////////////////////////////////////////
    // line to parse:
    // kerning first=121  second=44  amount=-7
    //////////////////////////////////////////////////////////////////////////

    // first
    int first;
    int index = line.find("first=");
    int index2 = line.find(' ', index);
    std::string value = line.substr(index, index2-index);
    sscanf(value.c_str(), "first=%d", &first);

    // second
    int second;
    index = line.find("second=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "second=%d", &second);

    // amount
    int amount;
    index = line.find("amount=");
    index2 = line.find(' ', index);
    value = line.substr(index, index2-index);
    sscanf(value.c_str(), "amount=%d", &amount);

    KerningHashElement *element = (KerningHashElement *)calloc( sizeof( *element ), 1 );
    element->amount = amount;
    element->key = (first<<16) | (second&0xffff);
    HASH_ADD_INT(m_pKerningDictionary,key, element);
}
//
//BMFontSprite
//
bool BMFSprite::init(){
	return true;
}

void BMFSprite::onEnter(){
	CCSprite::onEnter();
	SetChannel(0);
}


bool BMFSprite::initWithTexture(CCTexture2D *pTexture, const CCRect& rect, bool rotated = false){
	if (CCNodeRGBA::init())
    {
        m_pobBatchNode = NULL;
        
        m_bRecursiveDirty = false;
        setDirty(false);
        
        m_bOpacityModifyRGB = true;
        
		m_sBlendFunc.src = CC_BLEND_SRC; //GL_BLEND_SRC_ALPHA;
		m_sBlendFunc.dst = CC_BLEND_DST;
        
        m_bFlipX = m_bFlipY = false;
        
        // default transform anchor: center
        setAnchorPoint(ccp(0.5f, 0.5f));
        
        // zwoptex default values
        m_obOffsetPosition = CCPointZero;
        
        m_bHasChildren = false;
        
        // clean the Quad
        memset(&m_sQuad, 0, sizeof(m_sQuad));
        
        // Atlas: Color
        ccColor4B tmpColor = { 255, 255, 255, 255 };
        m_sQuad.bl.colors = tmpColor;
        m_sQuad.br.colors = tmpColor;
        m_sQuad.tl.colors = tmpColor;
        m_sQuad.tr.colors = tmpColor;
#ifndef mobileShaderCompileOnce
        // shader program 
#ifdef mobileShaderDebug
        CCGLProgram* glpg = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor);
#else
        CCGLProgram* glpg = CCShaderCache::sharedShaderCache()->programForKey("BMFShader");
#endif
        if(glpg == NULL)
		{
			glpg = new CCGLProgram();
			glpg->initWithVertexShaderFilename("Shaders/BMF1.vert", "Shaders/BMF1.frag");
			//glpg->initWithVertexShaderByteArray(vert1, frag1);
			glpg->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
			glpg->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
			glpg->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
			glpg->link();
			glpg->updateUniforms();
			CCShaderCache::sharedShaderCache()->addProgram(glpg, "BMFShader");
		}
		setShaderProgram(glpg);
#else
		setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor));
#endif
		//setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor));
  

        // update texture (calls updateBlendFunc)
        setTexture(pTexture);
        setTextureRect(rect, rotated, rect.size);
        
        // by default use "Self Render".
        // if the sprite is added to a batchnode, then it will automatically switch to "batchnode Render"
        setBatchNode(NULL);
        
		//u_fontChannel = getShaderProgram()->getUniformLocationForName("u_fontChannel");
		//GLfloat (*fontchannel) = glfarray;
		//getShaderProgram()->setUniformLocationWith4fv(u_fontChannel, fontchannel, 4);
		m_channel = 0;
		SetChannel(m_channel);
        return true;
    }
    else
    {
        return false;
    }
}

void BMFSprite::SetChannel(unsigned int channel){
	/*
	if(channel==1)
		channel=0;
	else if(channel==2)
		channel=1;
	else if(channel==4)
		channel=2;
	else if(channel==8)
		channel=3;
	*/
	channel = ((channel & 8) >> 3)*3 + ((channel & 4) >> 2)*2 + ((channel & 2) >> 1)*1;
	channel *= 4;
	setVertexZ(channel);
}

void BMFSprite::draw(){
	    CC_PROFILER_START_CATEGORY(kCCProfilerCategorySprite, "CCSprite - draw");

    CCAssert(!m_pobBatchNode, "If CCSprite is being rendered by CCSpriteBatchNode, CCSprite#draw SHOULD NOT be called");

    CC_NODE_DRAW_SETUP();
	ccGLBlendFunc( m_sBlendFunc.src, m_sBlendFunc.dst);

    ccGLBindTexture2D( m_pobTexture->getName() );
    ccGLEnableVertexAttribs( kCCVertexAttribFlag_PosColorTex );

#define kQuadSize sizeof(m_sQuad.bl)
#ifdef EMSCRIPTEN
    long offset = 0;
    setGLBufferData(&m_sQuad, 4 * kQuadSize, 0);
#else
    long offset = (long)&m_sQuad;
#endif // EMSCRIPTEN

    // vertex
    int diff = offsetof( ccV3F_C4B_T2F, vertices);
    glVertexAttribPointer(kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, kQuadSize, (void*) (offset + diff));

    // texCoods

    diff = offsetof( ccV3F_C4B_T2F, texCoords);
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, kQuadSize, (void*)(offset + diff));

    // color
    diff = offsetof( ccV3F_C4B_T2F, colors);
    glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, kQuadSize, (void*)(offset + diff));
 
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    CHECK_GL_ERROR_DEBUG();


#if CC_SPRITE_DEBUG_DRAW == 1
    // draw bounding box
    CCPoint vertices[4]={
        ccp(m_sQuad.tl.vertices.x,m_sQuad.tl.vertices.y),
        ccp(m_sQuad.bl.vertices.x,m_sQuad.bl.vertices.y),
        ccp(m_sQuad.br.vertices.x,m_sQuad.br.vertices.y),
        ccp(m_sQuad.tr.vertices.x,m_sQuad.tr.vertices.y),
    };
    ccDrawPoly(vertices, 4, true);
#elif CC_SPRITE_DEBUG_DRAW == 2
    // draw texture box
    CCSize s = this->getTextureRect().size;
    CCPoint offsetPix = this->getOffsetPosition();
    CCPoint vertices[4] = {
        ccp(offsetPix.x,offsetPix.y), ccp(offsetPix.x+s.width,offsetPix.y),
        ccp(offsetPix.x+s.width,offsetPix.y+s.height), ccp(offsetPix.x,offsetPix.y+s.height)
    };
    ccDrawPoly(vertices, 4, true);
#endif // CC_SPRITE_DEBUG_DRAW

    CC_INCREMENT_GL_DRAWS(1);

    CC_PROFILER_STOP_CATEGORY(kCCProfilerCategorySprite, "CCSprite - draw");

}


//
//LabelBMFont
//

//LabelBMFont - Purge Cache
void LabelBMFont::purgeCachedData()
{
    FNTConfigRemoveCache_ex();
}

LabelBMFont * LabelBMFont::create()
{
    LabelBMFont * pRet = new LabelBMFont();
    if (pRet && pRet->init())
    {
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return NULL;
}

LabelBMFont * LabelBMFont::create(const char *str, const char *fntFile, float width, CCTextAlignment alignment)
{
    return LabelBMFont::create(str, fntFile, width, alignment, CCPointZero);
}

LabelBMFont * LabelBMFont::create(const char *str, const char *fntFile, float width)
{
    return LabelBMFont::create(str, fntFile, width, kCCTextAlignmentLeft, CCPointZero);
}

LabelBMFont * LabelBMFont::create(const char *str, const char *fntFile)
{
    return LabelBMFont::create(str, fntFile, kCCLabelAutomaticWidth_ex, kCCTextAlignmentLeft, CCPointZero);
}

//LabelBMFont - Creation & Init
LabelBMFont *LabelBMFont::create(const char *str, const char *fntFile, float width/* = kCCLabelAutomaticWidth_ex*/, CCTextAlignment alignment/* = kCCTextAlignmentLeft*/, CCPoint imageOffset/* = CCPointZero*/)
{
    LabelBMFont *pRet = new LabelBMFont();
    if(pRet && pRet->initWithString(str, fntFile, width, alignment, imageOffset))
    {
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return NULL;
}

bool LabelBMFont::init()
{
    return initWithString(NULL, NULL, kCCLabelAutomaticWidth_ex, kCCTextAlignmentLeft, CCPointZero);
}

bool LabelBMFont::initWithString(const char *theString, const char *fntFile, float width/* = kCCLabelAutomaticWidth_ex*/, CCTextAlignment alignment/* = kCCTextAlignmentLeft*/, CCPoint imageOffset/* = CCPointZero*/)
{
    CCAssert(!m_pConfiguration, "re-init is no longer supported");
    CCAssert( (theString && fntFile) || (theString==NULL && fntFile==NULL), "Invalid params for LabelBMFont");
    
    CCTexture2D *texture = NULL;
    
    if (fntFile)
    {
        BMFontConfiguration *newConf = FNTConfigLoadFile_ex(fntFile);
        if (!newConf)
        {
            CCLOG("cocos2d: WARNING. LabelBMFont: Impossible to create font. Please check file: '%s'", fntFile);
            release();
            return false;
        }
        
        newConf->retain();
        CC_SAFE_RELEASE(m_pConfiguration);
        m_pConfiguration = newConf;
        
        m_sFntFile = fntFile; 
        texture = CCTextureCache::sharedTextureCache()->addImage(m_pConfiguration->getAtlasName());
    }
    else 
    {
        texture = new CCTexture2D();
        texture->autorelease();
    }

    if (theString == NULL)
    {
        theString = "";
    }

    if (initWithTexture(texture, strlen(theString)))//CCSpriteBatchNode::initWithTexture(texture, strlen(theString)))
    {
        m_fWidth = width;
        m_pAlignment = alignment;
        
        m_cDisplayedOpacity = m_cRealOpacity = 255;
		m_tDisplayedColor = m_tRealColor = ccWHITE;
        m_bCascadeOpacityEnabled = true;
        m_bCascadeColorEnabled = true;
        
        m_obContentSize = CCSizeZero;
        
        m_bIsOpacityModifyRGB = m_pobTextureAtlas->getTexture()->hasPremultipliedAlpha();
        m_obAnchorPoint = ccp(0.5f, 0.5f);
        
        m_tImageOffset = imageOffset;
        
        m_pReusedChar = new BMFSprite();
        m_pReusedChar->initWithTexture(m_pobTextureAtlas->getTexture(), CCRectMake(0, 0, 0, 0), false);
        m_pReusedChar->setBatchNode(this);
        
        this->setString(theString, true);
        
        return true;
    }
    return false;
}
bool LabelBMFont::initWithTexture(CCTexture2D *tex, unsigned int capacity){
	m_blendFunc.src = CC_BLEND_SRC;
    m_blendFunc.dst = CC_BLEND_DST;
    m_pobTextureAtlas = new CCTextureAtlas();

    if (0 == capacity)
    {
        capacity = kDefaultSpriteBatchCapacity;
    }
    
    m_pobTextureAtlas->initWithTexture(tex, capacity);

    // no lazy alloc in this node
    m_pChildren = new CCArray();
    m_pChildren->initWithCapacity(capacity);

	m_pobDescendants = new CCArray();
	m_pobDescendants->initWithCapacity(capacity);

	// shader program 
#ifdef mobileShaderDebug
    CCGLProgram* glpg = CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor);
#else
	CCGLProgram* glpg = CCShaderCache::sharedShaderCache()->programForKey("BMFShader");
#endif
	if(glpg == NULL)
	{
		glpg = new CCGLProgram();
		glpg->initWithVertexShaderFilename("Shaders/BMF1.vert", "Shaders/BMF1.frag");
		//glpg->initWithVertexShaderByteArray(vert1, frag1);
		glpg->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
		glpg->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
		glpg->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
		glpg->link();
		glpg->updateUniforms();
		CCShaderCache::sharedShaderCache()->addProgram(glpg, "BMFShader");
	}
	setShaderProgram(glpg);

	return true;




	/*


	    m_pobBatchNode = NULL;
        
        m_bRecursiveDirty = false;
        setDirty(false);
        
        m_bOpacityModifyRGB = true;
        
		m_sBlendFunc.src = CC_BLEND_SRC; //GL_BLEND_SRC_ALPHA;
		m_sBlendFunc.dst = CC_BLEND_DST;
        
        m_bFlipX = m_bFlipY = false;
        
        // default transform anchor: center
        setAnchorPoint(ccp(0.5f, 0.5f));
        
        // zwoptex default values
        m_obOffsetPosition = CCPointZero;
        
        m_bHasChildren = false;
        
        // clean the Quad
        memset(&m_sQuad, 0, sizeof(m_sQuad));
        
        // Atlas: Color
        ccColor4B tmpColor = { 255, 255, 255, 255 };
        m_sQuad.bl.colors = tmpColor;
        m_sQuad.br.colors = tmpColor;
        m_sQuad.tl.colors = tmpColor;
        m_sQuad.tr.colors = tmpColor;

        // shader program 
		CCGLProgram* glpg = CCShaderCache::sharedShaderCache()->programForKey("BMFShader");
		if(glpg == NULL)
		{
			glpg = new CCGLProgram();
			glpg->initWithVertexShaderFilename("Shaders/BMF.vert", "Shaders/BMF.frag");
			//glpg->initWithVertexShaderByteArray(vert1, frag1);
			glpg->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
			glpg->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
			glpg->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
			glpg->addAttribute("a_texCoord1", kCCVertexAttrib_TexCoords);
			glpg->link();
			glpg->updateUniforms();
			CCShaderCache::sharedShaderCache()->addProgram(glpg, "BMFShader");
		}
		setShaderProgram(glpg);
		//setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionTextureColor));
  

        // update texture (calls updateBlendFunc)
        setTexture(pTexture);
        setTextureRect(rect, rotated, rect.size);
        
        // by default use "Self Render".
        // if the sprite is added to a batchnode, then it will automatically switch to "batchnode Render"
        setBatchNode(NULL);
        
		u_fontChannel = getShaderProgram()->getUniformLocationForName("u_fontChannel");
		GLfloat (*fontchannel) = glfarray;
		getShaderProgram()->setUniformLocationWith4fv(u_fontChannel, fontchannel, 4);
		SetChannel(0);
        return true;
		*/
}

LabelBMFont::LabelBMFont()
: m_sString(NULL)
, m_sInitialString(NULL)
, m_pAlignment(kCCTextAlignmentCenter)
, m_fWidth(-1.0f)
, m_pConfiguration(NULL)
, m_bLineBreakWithoutSpaces(false)
, m_tImageOffset(CCPointZero)
, m_pReusedChar(NULL)
, m_cDisplayedOpacity(255)
, m_cRealOpacity(255)
, m_tDisplayedColor(ccWHITE)
, m_tRealColor(ccWHITE)
, m_bCascadeColorEnabled(true)
, m_bCascadeOpacityEnabled(true)
, m_bIsOpacityModifyRGB(false)
{

}

LabelBMFont::~LabelBMFont()
{
    CC_SAFE_RELEASE(m_pReusedChar);
    CC_SAFE_DELETE_ARRAY(m_sString);
    CC_SAFE_DELETE_ARRAY(m_sInitialString);
    CC_SAFE_RELEASE(m_pConfiguration);
}

// LabelBMFont - Atlas generation
int LabelBMFont::kerningAmountForFirst(unsigned short first, unsigned short second)
{
    int ret = 0;
    unsigned int key = (first<<16) | (second & 0xffff);

    if( m_pConfiguration->m_pKerningDictionary ) {
        KerningHashElement *element = NULL;
        HASH_FIND_INT(m_pConfiguration->m_pKerningDictionary, &key, element);        
        if(element)
            ret = element->amount;
    }
    return ret;
}

void LabelBMFont::createFontChars()
{
    int nextFontPositionX = 0;
    int nextFontPositionY = 0;
    unsigned short prev = -1;
    int kerningAmount = 0;

    CCSize tmpSize = CCSizeZero;

    int longestLine = 0;
    unsigned int totalHeight = 0;

    unsigned int quantityOfLines = 1;
    unsigned int stringLen = m_sString ? cc_wcslen(m_sString) : 0;
    if (stringLen == 0)
    {
        this->setContentSize(CC_SIZE_PIXELS_TO_POINTS(tmpSize));
        return;
    }

    set<unsigned int> *charSet = m_pConfiguration->getCharacterSet();

    for (unsigned int i = 0; i < stringLen - 1; ++i)
    {
        unsigned short c = m_sString[i];
        if (c == '\n')
        {
            quantityOfLines++;
        }
    }

    totalHeight = m_pConfiguration->m_nCommonHeight * quantityOfLines;
    nextFontPositionY = 0-(m_pConfiguration->m_nCommonHeight - m_pConfiguration->m_nCommonHeight * quantityOfLines);
    
    CCRect rect;
    BMFontDef fontDef;

    for (unsigned int i= 0; i < stringLen; i++)
    {
        unsigned short c = m_sString[i];

        if (c == '\n')
        {
            nextFontPositionX = 0;
            nextFontPositionY -= m_pConfiguration->m_nCommonHeight;
            continue;
        }
        
        if (charSet->find(c) == charSet->end())
        {
            CCLOGWARN("cocos2d::LabelBMFont: Attempted to use character not defined in this bitmap: %d", c);
            continue;      
        }

        kerningAmount = this->kerningAmountForFirst(prev, c);
        
        FontDefHashElement *element = NULL;

        // unichar is a short, and an int is needed on HASH_FIND_INT
        unsigned int key = c;
        HASH_FIND_INT(m_pConfiguration->m_pFontDefDictionary, &key, element);
        if (! element)
        {
            CCLOGWARN("cocos2d::LabelBMFont: characer not found %d", c);
            continue;
        }

        fontDef = element->fontDef;

        rect = fontDef.rect;
        rect = CC_RECT_PIXELS_TO_POINTS(rect);

        rect.origin.x += m_tImageOffset.x;
        rect.origin.y += m_tImageOffset.y;

        BMFSprite *fontChar;

        bool hasSprite = true;
        fontChar = (BMFSprite*)(this->getChildByTag(i));
        if(fontChar )
        {
            // Reusing previous Sprite
			fontChar->setVisible(true);
        }
        else
        {
            // New Sprite ? Set correct color, opacity, etc...
            if( 0 )
            {
				/* WIP: Doesn't support many features yet.
				 But this code is super fast. It doesn't create any sprite.
				 Ideal for big labels.
				 */
				fontChar = m_pReusedChar;
				fontChar->setBatchNode(NULL);
				hasSprite = false;
			}
            else
            {
                fontChar = new BMFSprite();
                fontChar->initWithTexture(m_pobTextureAtlas->getTexture(), rect, false);
                addChild(fontChar, i, i);
                fontChar->release();
			}
            
            // Apply label properties
			fontChar->setOpacityModifyRGB(m_bIsOpacityModifyRGB);
            
			// Color MUST be set before opacity, since opacity might change color if OpacityModifyRGB is on
			fontChar->updateDisplayedColor(m_tDisplayedColor);
			fontChar->updateDisplayedOpacity(m_cDisplayedOpacity);
        }

        // updating previous sprite
        fontChar->setTextureRect(rect, false, rect.size);

        // See issue 1343. cast( signed short + unsigned integer ) == unsigned integer (sign is lost!)
        int yOffset = m_pConfiguration->m_nCommonHeight - fontDef.yOffset;
        CCPoint fontPos = ccp( (float)nextFontPositionX + fontDef.xOffset + fontDef.rect.size.width*0.5f + kerningAmount,
            (float)nextFontPositionY + yOffset - rect.size.height*0.5f * CC_CONTENT_SCALE_FACTOR() );
        fontChar->setPosition(CC_POINT_PIXELS_TO_POINTS(fontPos));

        // update kerning
        nextFontPositionX += fontDef.xAdvance + kerningAmount;
        prev = c;

		//update font channel

		fontChar->SetChannel(fontDef.chnl);

        if (longestLine < nextFontPositionX)
        {
            longestLine = nextFontPositionX;
        }
        
        if (! hasSprite)
        {
            updateQuadFromSprite(fontChar, i);
        }
    }

    // If the last character processed has an xAdvance which is less that the width of the characters image, then we need
    // to adjust the width of the string to take this into account, or the character will overlap the end of the bounding
    // box
    if (fontDef.xAdvance < fontDef.rect.size.width)
    {
        tmpSize.width = longestLine + fontDef.rect.size.width - fontDef.xAdvance;
    }
    else
    {
        tmpSize.width = longestLine;
    }
    tmpSize.height = totalHeight;

    this->setContentSize(CC_SIZE_PIXELS_TO_POINTS(tmpSize));
}

//LabelBMFont - CCLabelProtocol protocol
void LabelBMFont::setString(const char *newString)
{
    this->setString(newString, true);
}

void LabelBMFont::setString(const char *newString, bool needUpdateLabel)
{
    if (newString == NULL) {
        newString = "";
    }
    if (needUpdateLabel) {
        m_sInitialStringUTF8 = newString;
    }
    unsigned short* utf16String = cc_utf8_to_utf16(newString);
    setString(utf16String, needUpdateLabel);
    CC_SAFE_DELETE_ARRAY(utf16String);
 }

void LabelBMFont::setString(unsigned short *newString, bool needUpdateLabel)
{
    if (!needUpdateLabel)
    {
        unsigned short* tmp = m_sString;
        m_sString = copyUTF16StringN(newString);
        CC_SAFE_DELETE_ARRAY(tmp);
    }
    else
    {
        unsigned short* tmp = m_sInitialString;
        m_sInitialString = copyUTF16StringN(newString);
        CC_SAFE_DELETE_ARRAY(tmp);
    }
    
    if (m_pChildren && m_pChildren->count() != 0)
    {
        CCObject* child;
        CCARRAY_FOREACH(m_pChildren, child)
        {
            CCNode* pNode = (CCNode*) child;
            if (pNode)
            {
                pNode->setVisible(false);
            }
        }
    }
    this->createFontChars();
    
    if (needUpdateLabel) {
        updateLabel();
    }
}

const char* LabelBMFont::getString(void)
{
    return m_sInitialStringUTF8.c_str();
}

void LabelBMFont::setCString(const char *label)
{
    setString(label);
}

//LabelBMFont - CCRGBAProtocol protocol
const ccColor3B& LabelBMFont::getColor()
{
    return m_tRealColor;
}

const ccColor3B& LabelBMFont::getDisplayedColor()
{
    return m_tDisplayedColor;
}

void LabelBMFont::setColor(const ccColor3B& color)
{
	m_tDisplayedColor = m_tRealColor = color;
	
	if( m_bCascadeColorEnabled ) {
		ccColor3B parentColor = ccWHITE;
        CCRGBAProtocol* pParent = dynamic_cast<CCRGBAProtocol*>(m_pParent);
        if (pParent && pParent->isCascadeColorEnabled())
        {
            parentColor = pParent->getDisplayedColor();
        }
        this->updateDisplayedColor(parentColor);
	}
}

GLubyte LabelBMFont::getOpacity(void)
{
    return m_cRealOpacity;
}

GLubyte LabelBMFont::getDisplayedOpacity(void)
{
    return m_cDisplayedOpacity;
}

/** Override synthesized setOpacity to recurse items */
void LabelBMFont::setOpacity(GLubyte opacity)
{
	m_cDisplayedOpacity = m_cRealOpacity = opacity;
    
	if( m_bCascadeOpacityEnabled ) {
		GLubyte parentOpacity = 255;
        CCRGBAProtocol* pParent = dynamic_cast<CCRGBAProtocol*>(m_pParent);
        if (pParent && pParent->isCascadeOpacityEnabled())
        {
            parentOpacity = pParent->getDisplayedOpacity();
        }
        this->updateDisplayedOpacity(parentOpacity);
	}
}

void LabelBMFont::setOpacityModifyRGB(bool var)
{
    m_bIsOpacityModifyRGB = var;
    if (m_pChildren && m_pChildren->count() != 0)
    {
        CCObject* child;
        CCARRAY_FOREACH(m_pChildren, child)
        {
            CCNode* pNode = (CCNode*) child;
            if (pNode)
            {
                CCRGBAProtocol *pRGBAProtocol = dynamic_cast<CCRGBAProtocol*>(pNode);
                if (pRGBAProtocol)
                {
                    pRGBAProtocol->setOpacityModifyRGB(m_bIsOpacityModifyRGB);
                }
            }
        }
    }
}
bool LabelBMFont::isOpacityModifyRGB()
{
    return m_bIsOpacityModifyRGB;
}

void LabelBMFont::updateDisplayedOpacity(GLubyte parentOpacity)
{
	m_cDisplayedOpacity = m_cRealOpacity * parentOpacity/255.0;
    
	CCObject* pObj;
	CCARRAY_FOREACH(m_pChildren, pObj)
    {
        CCSprite *item = dynamic_cast<CCSprite*>(pObj);
        if(item!=NULL)
		item->updateDisplayedOpacity(m_cDisplayedOpacity);
	}
}

void LabelBMFont::updateDisplayedColor(const ccColor3B& parentColor)
{
	m_tDisplayedColor.r = m_tRealColor.r * parentColor.r/255.0;
	m_tDisplayedColor.g = m_tRealColor.g * parentColor.g/255.0;
	m_tDisplayedColor.b = m_tRealColor.b * parentColor.b/255.0;
    
    CCObject* pObj;
	CCARRAY_FOREACH(m_pChildren, pObj)
    {
        CCSprite *item = dynamic_cast<CCSprite*>(pObj);
        if(item!=NULL)
		item->updateDisplayedColor(m_tDisplayedColor);
	}
}

bool LabelBMFont::isCascadeColorEnabled()
{
    return false;
}

void LabelBMFont::setCascadeColorEnabled(bool cascadeColorEnabled)
{
    m_bCascadeColorEnabled = cascadeColorEnabled;
}

bool LabelBMFont::isCascadeOpacityEnabled()
{
    return false;
}

void LabelBMFont::setCascadeOpacityEnabled(bool cascadeOpacityEnabled)
{
    m_bCascadeOpacityEnabled = cascadeOpacityEnabled;
}

// LabelBMFont - AnchorPoint
void LabelBMFont::setAnchorPoint(const CCPoint& point)
{
    if( ! point.equals(m_obAnchorPoint))
    {
        CCSpriteBatchNode::setAnchorPoint(point);
        updateLabel();
    }
}

// LabelBMFont - Alignment
void LabelBMFont::updateLabel()
{
    this->setString(m_sInitialString, false);

    if (m_fWidth > 0)
    {
        // Step 1: Make multiline
        vector<unsigned short> str_whole = cc_utf16_vec_from_utf16_str(m_sString);
        unsigned int stringLength = str_whole.size();
        vector<unsigned short> multiline_string;
        multiline_string.reserve( stringLength );
        vector<unsigned short> last_word;
        last_word.reserve( stringLength );

        unsigned int line = 1, i = 0;
        bool start_line = false, start_word = false;
        float startOfLine = -1, startOfWord = -1;
        int skip = 0;

        CCArray* children = getChildren();
        for (unsigned int j = 0; j < children->count(); j++)
        {
            BMFSprite* characterSprite;
            unsigned int justSkipped = 0;
			
            while (!(characterSprite = (BMFSprite*)this->getChildByTag(j + skip + justSkipped)))
            {
                justSkipped++;
            }

            skip += justSkipped;
            
            if (!characterSprite->isVisible())
                continue;

            if (i >= stringLength)
                break;

            unsigned short character = str_whole[i];

            if (!start_word)
            {
                startOfWord = getLetterPosXLeft( characterSprite );
                start_word = true;
            }
            if (!start_line)
            {
                startOfLine = startOfWord;
                start_line = true;
            }

            // Newline.
            if (character == '\n')
            {
                cc_utf8_trim_ws(&last_word);

                last_word.push_back('\n');
                multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());
                last_word.clear();
                start_word = false;
                start_line = false;
                startOfWord = -1;
                startOfLine = -1;
                i+=justSkipped;
                line++;

                if (i >= stringLength)
                    break;

                character = str_whole[i];

                if (!startOfWord)
                {
                    startOfWord = getLetterPosXLeft( characterSprite );
                    start_word = true;
                }
                if (!startOfLine)
                {
                    startOfLine  = startOfWord;
                    start_line = true;
                }
            }

            // Whitespace.
            if (isspace_unicode(character))
            {
                last_word.push_back(character);
                multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());
                last_word.clear();
                start_word = false;
                startOfWord = -1;
                i++;
                continue;
            }

            // Out of bounds.
            if ( getLetterPosXRight( characterSprite ) - startOfLine > m_fWidth )
            {
                if (!m_bLineBreakWithoutSpaces)
                {
                    last_word.push_back(character);

                    int found = cc_utf8_find_last_not_char(multiline_string, ' ');
                    if (found != -1)
                        cc_utf8_trim_ws(&multiline_string);
                    else
                        multiline_string.clear();

                    if (multiline_string.size() > 0)
                        multiline_string.push_back('\n');

                    line++;
                    start_line = false;
                    startOfLine = -1;
                    i++;
                }
                else
                {
                    cc_utf8_trim_ws(&last_word);

                    last_word.push_back('\n');
                    multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());
                    last_word.clear();
                    start_word = false;
                    start_line = false;
                    startOfWord = -1;
                    startOfLine = -1;
                    line++;

                    if (i >= stringLength)
                        break;

                    if (!startOfWord)
                    {
                        startOfWord = getLetterPosXLeft( characterSprite );
                        start_word = true;
                    }
                    if (!startOfLine)
                    {
                        startOfLine  = startOfWord;
                        start_line = true;
                    }

                    j--;
                }

                continue;
            }
            else
            {
                // Character is normal.
                last_word.push_back(character);
                i++;
                continue;
            }
        }

        multiline_string.insert(multiline_string.end(), last_word.begin(), last_word.end());

        int size = multiline_string.size();
        unsigned short* str_new = new unsigned short[size + 1];

        for (int i = 0; i < size; ++i)
        {
            str_new[i] = multiline_string[i];
        }

        str_new[size] = '\0';

        this->setString(str_new, false);
        
        CC_SAFE_DELETE_ARRAY(str_new);
    }

    // Step 2: Make alignment
    if (m_pAlignment != kCCTextAlignmentLeft)
    {
        int i = 0;

        int lineNumber = 0;
        int str_len = cc_wcslen(m_sString);
        vector<unsigned short> last_line;
        for (int ctr = 0; ctr <= str_len; ++ctr)
        {
            if (m_sString[ctr] == '\n' || m_sString[ctr] == 0)
            {
                float lineWidth = 0.0f;
                unsigned int line_length = last_line.size();
				// if last line is empty we must just increase lineNumber and work with next line
                if (line_length == 0)
                {
                    lineNumber++;
                    continue;
                }
                int index = i + line_length - 1 + lineNumber;
                if (index < 0) continue;

                BMFSprite* lastChar = (BMFSprite*)getChildByTag(index);
                if ( lastChar == NULL )
                    continue;

                lineWidth = lastChar->getPosition().x + lastChar->getContentSize().width/2.0f;

                float shift = 0;
                switch (m_pAlignment)
                {
                case kCCTextAlignmentCenter:
                    shift = getContentSize().width/2.0f - lineWidth/2.0f;
                    break;
                case kCCTextAlignmentRight:
                    shift = getContentSize().width - lineWidth;
                    break;
                default:
                    break;
                }

                if (shift != 0)
                {
                    for (unsigned j = 0; j < line_length; j++)
                    {
                        index = i + j + lineNumber;
                        if (index < 0) continue;

                        BMFSprite* characterSprite = (BMFSprite*)getChildByTag(index);
                        characterSprite->setPosition(ccpAdd(characterSprite->getPosition(), ccp(shift, 0.0f)));
                    }
                }

                i += line_length;
                lineNumber++;

                last_line.clear();
                continue;
            }

            last_line.push_back(m_sString[ctr]);
        }
    }
}

// LabelBMFont - Alignment
void LabelBMFont::setAlignment(CCTextAlignment alignment)
{
    this->m_pAlignment = alignment;
    updateLabel();
}

void LabelBMFont::setWidth(float width)
{
    this->m_fWidth = width;
    updateLabel();
}

void LabelBMFont::setLineBreakWithoutSpace( bool breakWithoutSpace )
{
    m_bLineBreakWithoutSpaces = breakWithoutSpace;
    updateLabel();
}

void LabelBMFont::setScale(float scale)
{
    CCSpriteBatchNode::setScale(scale);
    updateLabel();
}

void LabelBMFont::setScaleX(float scaleX)
{
    CCSpriteBatchNode::setScaleX(scaleX);
    updateLabel();
}

void LabelBMFont::setScaleY(float scaleY)
{
    CCSpriteBatchNode::setScaleY(scaleY);
    updateLabel();
}

float LabelBMFont::getLetterPosXLeft( CCSprite* sp )
{
    return sp->getPosition().x * m_fScaleX - (sp->getContentSize().width * m_fScaleX * sp->getAnchorPoint().x);
}

float LabelBMFont::getLetterPosXRight( CCSprite* sp )
{
    return sp->getPosition().x * m_fScaleX + (sp->getContentSize().width * m_fScaleX * sp->getAnchorPoint().x);
}

// LabelBMFont - FntFile
void LabelBMFont::setFntFile(const char* fntFile)
{
    if (fntFile != NULL && strcmp(fntFile, m_sFntFile.c_str()) != 0 )
    {
        BMFontConfiguration *newConf = FNTConfigLoadFile_ex(fntFile);

        CCAssert( newConf, "LabelBMFont: Impossible to create font. Please check file");

        m_sFntFile = fntFile;

        CC_SAFE_RETAIN(newConf);
        CC_SAFE_RELEASE(m_pConfiguration);
        m_pConfiguration = newConf;

        this->setTexture(CCTextureCache::sharedTextureCache()->addImage(m_pConfiguration->getAtlasName()));
        this->createFontChars();
    }
}

const char* LabelBMFont::getFntFile()
{
    return m_sFntFile.c_str();
}

BMFontConfiguration* LabelBMFont::getConfiguration() const
{
	return m_pConfiguration;
}

//LabelBMFont - Debug draw
#if CC_LABELBMFONT_DEBUG_DRAW
void LabelBMFont::draw()
{
    CCSpriteBatchNode::draw();
    const CCSize& s = this->getContentSize();
    CCPoint vertices[4]={
        ccp(0,0),ccp(s.width,0),
        ccp(s.width,s.height),ccp(0,s.height),
    };
    ccDrawPoly(vertices, 4, true);
}
#endif // CC_LABELBMFONT_DEBUG_DRAW
/*
void LabelBMFont::draw(void)
{
    CC_PROFILER_START("CCSpriteBatchNode - draw");
    // Optimization: Fast Dispatch
    if( m_pobTextureAtlas->getTotalQuads() == 0 )
    {
        return;
    }

    CC_NODE_DRAW_SETUP();

    arrayMakeObjectsPerformSelector(m_pChildren, updateTransform, CCSprite*);
    ccGLBlendFunc( m_blendFunc.src, m_blendFunc.dst );

    m_pobTextureAtlas->drawQuads();

    CC_PROFILER_STOP("CCSpriteBatchNode - draw");
}
*/
/**   animation  */
void LabelBMFont::runRandomAni(){
	for(int i=0;i<m_pChildren->count();++i){
		BMFSprite* bs =(BMFSprite*) m_pChildren->objectAtIndex(i);
		CCPoint origin = bs->getPosition();
		time(NULL);
		float x = CCRANDOM_MINUS1_1()*400;
		bs->setPositionX(x + bs->getPositionX());
		time(NULL);
		float y = CCRANDOM_MINUS1_1()*400;
		bs->setPositionY( y + bs->getPositionY());
		bs->setScale(CCRANDOM_0_1()*3 + 1);
		//bs->runAction(CCSequence::create(CCMoveTo::create(CCRANDOM_0_1() * 5, origin),CCScaleTo::create(1,1,1), NULL));
		bs->runAction(CCMoveTo::create(CCRANDOM_0_1() * 5, origin));
		bs->runAction(CCScaleTo::create(1,1,1));
	}
}
NS_BMF_END
