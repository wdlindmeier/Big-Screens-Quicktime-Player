#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"
#include "cinder/qtime/QuickTime.h"
#include "cinder/Utilities.h"
#include "cinder/Xml.h"
#include "cinder/System.h"
#include "OscSender.h"
#include "OscListener.h"

using namespace ci;
using namespace ci::app;
using namespace std;

const static bool UseFrameSyncing = false;
const static std::string kOSCAddressSyncTime = "/sync/time";
const static std::string kOSCAddressSyncFrame = "/sync/frame";

class BigScreensVideoPlayerApp : public AppNative {
  public:
    void prepareSettings(Settings *settings);
	void setup();
	void update();
	void draw();
    void sync();
    void keyDown(KeyEvent event);
    void loadSettings(const fs::path & settingsPath);
    void loadMovieFile( const fs::path &moviePath );
    
    gl::Texture				mFrameTexture, mInfoTexture;
	qtime::MovieGlRef		mMovie;
    
    // Positioning
    ci::Rectf               mLocalViewportRect;
    ci::Vec2i               mMasterSize;
    
    // Connection
    int                     mListenPort;
    vector<int>             mSendPorts;
    
    // OSC
    bool                    mIsClock;
	osc::Listener           mClockListener;
	vector<osc::Sender>     mClockSenders;
    
    // Settings
    bool                    mLoadedSettings;
};

#pragma mark - Setup

void BigScreensVideoPlayerApp::prepareSettings(Settings *settings)
{
    settings->setBorderless();
}

void BigScreensVideoPlayerApp::setup()
{
    mLoadedSettings = false;
    
    fs::path settingsPath;
    
#ifndef CLIENT_ID
    // Using 1 client across machines:
    // Put the xml file in the same directory as the app.
    settingsPath = getAppPath() / ".." / ("settings.xml");
#else
    // Using multiple clients on the same machine (e.g. running from XCode).
    settingsPath = getAssetPath("settings."+std::to_string(CLIENT_ID)+".xml");
#endif
    
    console() << "Loading settings from " << settingsPath.string() << std::endl;
    loadSettings(settingsPath);
    
    if (UseFrameSyncing)
    {
        console() << "Syncing with frames." << std::endl;
    }
    else
    {
        console() << "Syncing with time." << std::endl;
    }
    
    if (mIsClock)
    {
        console() << "Target is clock." << std::endl;
        
        // Sender is multicast
        string host = System::getIpAddress();
        if(host.rfind( '.' ) != string::npos)
            host.replace(host.rfind( '.' ) + 1, 3, "255");
        
        for (int i = 0; i < mSendPorts.size(); ++i)
        {
            int port = mSendPorts[i];
            console() << "Sending to port " << port << std::endl;
            osc::Sender clockSender;
            clockSender.setup(host, port, true);
            mClockSenders.push_back(clockSender);
        }
    }
    else
    {
        console() << "Target is slave." << std::endl;
        console() << "Listening to port " << mListenPort << std::endl;
        mClockListener.setup(mListenPort);
    }    
}

#pragma mark - OSC

void BigScreensVideoPlayerApp::sync()
{
    if (mIsClock)
    {
        // Sync every second
        int elapsedFrames = getElapsedFrames();
        if (elapsedFrames % (int)getFrameRate() == 0)
        {
            osc::Message message;
            if (UseFrameSyncing)
            {
                float frameRatio = mMovie->getFramerate() / getFrameRate();
                int currentFrame = ((int)(elapsedFrames * frameRatio) % mMovie->getNumFrames());
                // Make sure this movie is on the right frame
                mMovie->seekToFrame(currentFrame);
                // Send the "next" frame
                int sendFrame = currentFrame;// + (1 * frameRatio);
                message.addIntArg(sendFrame);
                message.setAddress(kOSCAddressSyncFrame);
            }
            else
            {
                // Should this seek slightly ahead?...
                // Maybe we should poll the average offset between frames.
                message.addFloatArg(mMovie->getCurrentTime());
                message.setAddress(kOSCAddressSyncTime);
            }
            
            for (int i = 0; i < mClockSenders.size(); ++i)
            {
                mClockSenders[i].sendMessage(message);
            }
        }
    }
    else
    {
        while( mClockListener.hasWaitingMessages() )
        {
            osc::Message message;
            mClockListener.getNextMessage( &message );
            
            if (message.getAddress() == kOSCAddressSyncTime)
            {
                if (message.getArgType(0) == osc::TYPE_FLOAT)
                {
                    float gotoTime = message.getArgAsFloat(0);
                    console() << "Seek to time " << gotoTime << std::endl;
                    mMovie->seekToTime(gotoTime);
                }
                else
                {
                    console() << "ERROR: Argument must be a float" << std::endl;
                }
            }
            else if (message.getAddress() == kOSCAddressSyncFrame)
            {
                if (message.getArgType(0) == osc::TYPE_INT32)
                {
                    int gotoFrame = message.getArgAsInt32(0);
                    console() << "Seek to frame " << gotoFrame << std::endl;
                    mMovie->seekToFrame(gotoFrame);
                }
                else
                {
                    console() << "ERROR: Argument must be a float" << std::endl;
                }
            }
            else
            {
                console() << "WARN: Unknown message address: " << message.getAddress() << std::endl;
            }
        }
    }
}

#pragma mark - Loop

void BigScreensVideoPlayerApp::update()
{
	if(mMovie)
    {
		mFrameTexture = mMovie->getTexture();
        sync();
    }
}

void BigScreensVideoPlayerApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
    gl::color(1,1,1);

    if (mLoadedSettings)
    {
        if (mFrameTexture)
        {
            glEnable(GL_SCISSOR_TEST);
            
            glScissor(0, 0,
                      mLocalViewportRect.getWidth(),
                      mLocalViewportRect.getHeight());
            
            gl::setMatricesWindow(mLocalViewportRect.getWidth(),
                                  mLocalViewportRect.getHeight());
            
            glTranslatef(mLocalViewportRect.getX1() * -1,
                         mLocalViewportRect.getY1() * -1,
                         0);

            // This should maybe be another setting.
            // E.g. video placement, alignment, scale
            Rectf rectFrame(0, 0, mFrameTexture.getWidth(), mFrameTexture.getHeight());
            
            gl::draw( mFrameTexture, rectFrame  );
            
            gl::disable(GL_SCISSOR_TEST);
        }
    }
    else
    {
        gl::enableAlphaBlending();
        gl::drawString("ERROR: Could not load settings file.", Vec2f(20, 20));
        gl::disableAlphaBlending();
    }
}

#pragma mark - Input

void BigScreensVideoPlayerApp::keyDown(KeyEvent event)
{
    switch (event.getChar())
    {
        case 'f':
            setFullScreen(isFullScreen());
            break;
    }
}

#pragma mark - Settings

void BigScreensVideoPlayerApp::loadSettings(const fs::path & settingsPath)
{
    if (!fs::exists(settingsPath))
    {
        console() << "ERROR: Could not find settings file" << std::endl;
        return;
    }
    
    XmlTree settingsDoc(loadFile(settingsPath));
    
    mLoadedSettings = true;
    
    try
    {
        XmlTree node = settingsDoc.getChild( "settings/movie_path" );
        string moviePathString = node.getValue<string>();
        fs::path moviePath(moviePathString);
        if(!fs::exists(moviePath))
        {
            console() << "ERROR: Movie doesn't exist at path " << moviePath.string() << std::endl;
            return;
        }
        loadMovieFile( moviePath );
    }
    catch (XmlTree::ExcChildNotFound e)
    {
        console() << "ERROR: Could not find movie_path node in settings file." << std::endl;
    }
    
    try
    {
        XmlTree clockNode = settingsDoc.getChild( "settings/is_clock" );
        string boolStr = clockNode.getValue<string>();
        std::transform(boolStr.begin(), boolStr.end(), boolStr.begin(), ::tolower);
        mIsClock = (boolStr == "true");
    }
    catch (XmlTree::ExcChildNotFound e)
    {
        console() << "ERROR: Could not find is_clock node in settings file." << std::endl;
    }

    if (mIsClock)
    {
        try
        {
            for( XmlTree::ConstIter portIt = settingsDoc.begin("settings/send_port"); portIt != settingsDoc.end(); ++portIt )
            {
                mSendPorts.push_back(portIt->getValue<int>());
            }
        }
        catch (XmlTree::ExcChildNotFound e)
        {
            console() << "ERROR: Could not find server port settings." << std::endl;
        }
        
        if (mSendPorts.size() == 0)
        {
            console() << "ERROR: Could not find send_port in settings." << std::endl;
        }
    }
    else
    {
        try
        {
            XmlTree listenNode = settingsDoc.getChild( "settings/listen_port" );
            mListenPort = listenNode.getValue<int>();
        }
        catch (XmlTree::ExcChildNotFound e)
        {
            console() << "ERROR: Could not find listen_port node in settings file." << std::endl;
        }
    }    

    try
    {
        XmlTree widthNode = settingsDoc.getChild( "settings/local_dimensions/width" );
        XmlTree heightNode = settingsDoc.getChild( "settings/local_dimensions/height" );
        XmlTree xNode = settingsDoc.getChild( "settings/local_location/x" );
        XmlTree yNode = settingsDoc.getChild( "settings/local_location/y" );
        int width = widthNode.getValue<int>();
        int height = heightNode.getValue<int>();
        int x = xNode.getValue<int>();
        int y = yNode.getValue<int>();
        mLocalViewportRect = Rectf( x, y, x+width, y+height );
        
        // Force the window size based on the settings XML.
        setWindowSize(width, height);
    }
    catch (XmlTree::ExcChildNotFound e)
    {
        // Async controller doesn't need to know about the dimensions
        console() << "ERROR: Could not find local dimensions settings." << std::endl;
    }
    
    try
    {
        XmlTree widthNode = settingsDoc.getChild( "settings/master_dimensions/width" );
        XmlTree heightNode = settingsDoc.getChild( "settings/master_dimensions/height" );
        int width = widthNode.getValue<int>();
        int height = heightNode.getValue<int>();
        mMasterSize = Vec2i(width, height);
    }
    catch (XmlTree::ExcChildNotFound e)
    {
        // Async controller doesn't need to know about the dimensions
        console() << "ERROR: Could not find master dimensions settings" << std::endl;
    }
    
    try
    {
        XmlTree fullscreenNode = settingsDoc.getChild("settings/go_fullscreen");
        string boolStr = fullscreenNode.getValue<string>();
        std::transform(boolStr.begin(), boolStr.end(), boolStr.begin(), ::tolower);
        if (boolStr == "true")
        {
            setFullScreen(true);
        }
    }
    catch (XmlTree::ExcChildNotFound e)
    {
        // Not required
    }
    
    try
    {
        XmlTree fullscreenNode = settingsDoc.getChild("settings/offset_window");
        string boolStr = fullscreenNode.getValue<string>();
        std::transform(boolStr.begin(), boolStr.end(), boolStr.begin(), ::tolower);
        if (boolStr == "true")
        {
            setWindowPos(Vec2i(mLocalViewportRect.x1, mLocalViewportRect.y1));
        }
    }
    catch (XmlTree::ExcChildNotFound e)
    {
        // Not required
    }
}

void BigScreensVideoPlayerApp::loadMovieFile( const fs::path &moviePath )
{
	try {
		// load up the movie, set it to loop, and begin playing
		mMovie = qtime::MovieGl::create( moviePath );
		mMovie->setLoop();
		mMovie->play();
	}
	catch( Exception e )
    {
        console() << "EXCEPTION: " << e.what() << std::endl;
		console() << "Unable to load the movie." << std::endl;
		mMovie->reset();
	}
    
	mFrameTexture.reset();
}

CINDER_APP_NATIVE( BigScreensVideoPlayerApp, RendererGl )
