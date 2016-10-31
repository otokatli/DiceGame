//==============================================================================
/*
    \author    Ozan Tokatli
*/
//==============================================================================

//==============================================================================
/*

DiceGame:    application.cpp

Matching the orientation of a dice wrt a reference dice using a haptic interface

Features:
- Reference dice is generated with no haptics
- Actual dice (the manipulated one) is created with a bounding sphere attached
- The bounding sphere can be shown or hidden
- GLUT menu is created and attached to right mouse click
- Left mouse click is assigned to rotating the camera
- Space key (key = 32) is used to load a new reference dice orientation

\todo
- Data logging
- Multi-finger support

*/
//==============================================================================

//------------------------------------------------------------------------------
#include "chai3d.h"
#include "block_linked_list.h"
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
//------------------------------------------------------------------------------
#ifndef MACOSX
#include "GL/glut.h"
#else
#include "GLUT/glut.h"
#endif
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// GENERAL SETTINGS
//------------------------------------------------------------------------------

// stereo Mode
/*
    C_STEREO_DISABLED:            Stereo is disabled 
    C_STEREO_ACTIVE:              Active stereo for OpenGL NVDIA QUADRO cards
    C_STEREO_PASSIVE_LEFT_RIGHT:  Passive stereo where L/R images are rendered next to each other
    C_STEREO_PASSIVE_TOP_BOTTOM:  Passive stereo where L/R images are rendered above each other
*/
cStereoMode stereoMode = C_STEREO_DISABLED;

// fullscreen mode
bool fullscreen = false;

// mirrored display
bool mirroredDisplay = false;


//------------------------------------------------------------------------------
// DECLARED VARIABLES
//------------------------------------------------------------------------------

// a world that contains all objects of the virtual environment
cWorld* world;

// a camera to render the world in the window display
cCamera* camera;

// a light source to illuminate the objects in the world
cDirectionalLight *light;

// a haptic device handler
cHapticDeviceHandler* handler;

// a pointer to the current haptic device
cGenericHapticDevicePtr hapticDevice;

// a label to display the rate [Hz] at which the simulation is running
cLabel* labelHapticRate;

// a small sphere (cursor) representing the haptic device 
cToolCursor* tool;

// reference dice model
cMultiMesh* refDice;

// actual dice model (manipulated by the user)
cMultiMesh* actDice;

// bounding sphere of the actual dice
cMesh* boundingSphere;

// virtual button
cMesh* virtualButton;

// selected object
cGenericObject* selectedObject = NULL;

// flag to indicate if the haptic simulation currently running
bool simulationRunning = false;

// flag to indicate if the haptic simulation has terminated
bool simulationFinished = false;

// frequency counter to measure the simulation haptic rate
cFrequencyCounter frequencyCounter;

// last mouse position
int mouseX;
int mouseY;

// information about computer screen and GLUT display window
int screenW;
int screenH;
int windowW;
int windowH;
int windowPosX;
int windowPosY;

// scaling of the virtual objects
const double scale = 1.0;

// radius of the bounding sphere
double radii;

// user name/participant id
char userName[256];

// mouse button states
bool mouseLeftClick = false;
bool mouseRightClick = false;

// virtual button state
bool virtualButtonDown = false;

// contact state of virtual button
bool previousContactState = false;

struct HapticData{
	double    time;
	cMatrix3d refDiceOrientation;
	cVector3d actDicePos;
	cMatrix3d actDiceOrientation;
	cMatrix3d deviceOrientation;
	cVector3d devicePos;
	cVector3d deviceVel;
};

// buffer for storing data temporarily
block_linked_list<HapticData, (size_t)1000> dataBuffer;

// file to log data
FILE* dataFile;

// clock for measuring the timing of the experiment
cPrecisionClock timer;

//------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//------------------------------------------------------------------------------

// callback when the window display is resized
void resizeWindow(int w, int h);

// callback when a key is pressed
void keySelect(unsigned char key, int x, int y);

// callback to handle mouse click
void mouseClick(int button, int state, int x, int y);

// callback to handle mouse motion when button is pressed
void mouseMove(int x, int y);

// callback to render graphic scene
void updateGraphics(void);

// callback of GLUT timer
void graphicsTimer(int data);

// function that closes the application
void close(void);

// main haptics simulation loop
void updateHaptics(void);

// application menu
void createMenu(void);

// process the event of the menu
void processMenuEvents(int option);

// callback to log data
void logData(void);

// callback to flush logged data
void flushData(void);

// Reset object position and orientation
void resetWorld(void);

enum cMode
{
	IDLE,
	SELECTION
};

enum cVirtualMode
{
	vmIDLE,
	vmCONTACT
};

enum menuItem
{
	FULL_SCREEN,
	EXIT_APP,
	MIRROR_DISPLAY,
	BOUNDING_SPHERE,
	SEPARATOR,
	RESET_WORLD
};


//==============================================================================
/*
    
*/
//==============================================================================

int main(int argc, char* argv[])
{
    //--------------------------------------------------------------------------
    // INITIALIZATION
    //--------------------------------------------------------------------------

    cout << endl;
    cout << "-----------------------------------" << endl;
    cout << "Dice Game" << endl;
	cout << "   _______" << endl;
	cout << "  /\\ o o o\\" << endl;
	cout << " /o \\ o o o\\_______" << endl;
	cout << "<    >------>   o /|" << endl;
	cout << " \\ o/  o   /_____/o|" << endl;
	cout << "  \\/______/     |oo|" << endl;
	cout << "        |   o   |o/" << endl;
	cout << "        |_______|/" << endl;
    cout << "-----------------------------------" << endl << endl << endl;
    cout << "Keyboard Options:" << endl << endl;
    cout << "[f] - Enable/Disable full screen mode" << endl;
    cout << "[m] - Enable/Disable vertical mirroring" << endl;
    cout << "[x] - Exit application" << endl;
    cout << endl << endl;

	cout << "Name of the student/participant id:";
	//cin.get(userName, 256);
	cout << endl << endl;

	//--------------------------------------------------------------------------
	// OPEN FILE FOR DATA RECORDING
	//--------------------------------------------------------------------------
	//dataFile = fopen(strcat(userName, ".hdata"), "wb");
	dataFile = fopen("data.hdata", "wb");
	if (dataFile == 0)
	{
		cerr << "Error opening file";
		return -1;
	}

    //--------------------------------------------------------------------------
    // OPENGL - WINDOW DISPLAY
    //--------------------------------------------------------------------------

    // initialize GLUT
    glutInit(&argc, argv);

    // retrieve  resolution of computer display and position window accordingly
    screenW = glutGet(GLUT_SCREEN_WIDTH);
    screenH = glutGet(GLUT_SCREEN_HEIGHT);
    windowW = (int)(0.8 * screenH);
    windowH = (int)(0.5 * screenH);
    windowPosY = (screenH - windowH) / 2;
    windowPosX = windowPosY; 

    // initialize the OpenGL GLUT window
    glutInitWindowPosition(windowPosX, windowPosY);
    glutInitWindowSize(windowW, windowH);

    if (stereoMode == C_STEREO_ACTIVE)
        glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE | GLUT_STEREO);
    else
        glutInitDisplayMode(GLUT_RGB | GLUT_DEPTH | GLUT_DOUBLE);

    // create display context and initialize GLEW library
    glutCreateWindow(argv[0]);

#ifdef GLEW_VERSION
    // initialize GLEW
    glewInit();
#endif

    // setup GLUT options
    glutDisplayFunc(updateGraphics);
    glutKeyboardFunc(keySelect);
	glutMouseFunc(mouseClick);
	glutMotionFunc(mouseMove);
    glutReshapeFunc(resizeWindow);
    glutSetWindowTitle("Dice Game");
	createMenu();

    // set fullscreen mode
    if (fullscreen)
    {
        glutFullScreen();
    }


    //--------------------------------------------------------------------------
    // WORLD - CAMERA - LIGHTING
    //--------------------------------------------------------------------------

    // create a new world.
    world = new cWorld();

    // set the background color of the environment
    world->m_backgroundColor.setBlack();

    // create a camera and insert it into the virtual world
    camera = new cCamera(world);
    world->addChild(camera);

	// define a basis in spherical coordinates for the camera
	camera->setSphericalReferences(cVector3d(0, 0, 0),    // origin
		cVector3d(0, 0, 1),    // zenith direction
		cVector3d(1, 0, 0));   // azimuth direction

	camera->setSphericalDeg(4.0,    // spherical coordinate radius
		0,      // spherical coordinate azimuth angle
		0);     // spherical coordinate polar angle

    // set the near and far clipping planes of the camera
    camera->setClippingPlanes(0.01, 10.0);

    // set stereo mode
    camera->setStereoMode(stereoMode);

    // set stereo eye separation and focal length (applies only if stereo is enabled)
    camera->setStereoEyeSeparation(0.01);
    camera->setStereoFocalLength(0.5);

    // set vertical mirrored display mode
    camera->setMirrorVertical(mirroredDisplay);

    // create a directional light source
    light = new cDirectionalLight(world);

    // insert light source inside world
    world->addChild(light);

    // enable light source
    light->setEnabled(true);

    // define direction of light beam
    light->setDir(0.0, -1.0, -1.0); 


    //--------------------------------------------------------------------------
    // HAPTIC DEVICE
    //--------------------------------------------------------------------------

    // create a haptic device handler
    handler = new cHapticDeviceHandler();

    // get a handle to the first haptic device
    handler->getDevice(hapticDevice, 0);

    // open a connection to haptic device
    hapticDevice->open();

    // calibrate device (if necessary)
    hapticDevice->calibrate();

    // retrieve information about the current haptic device
    cHapticDeviceInfo info = hapticDevice->getSpecifications();

    // if the device has a gripper, enable the gripper to simulate a user switch
    hapticDevice->setEnableGripperUserSwitch(true);

	// create a tool (cursor) and insert into the world
	tool = new cToolCursor(world);
	world->addChild(tool);

	// connect the haptic device to the virtual tool
	tool->setHapticDevice(hapticDevice);

	// define the radius of the tool (sphere)
	double toolRadius = 0.1;
	tool->setRadius(toolRadius);

	// map the physical workspace of the haptic device to a larger virtual workspace.
	tool->setWorkspaceRadius(1.2);

	// enable if objects in the scene are going to rotate of translate
	// or possibly collide against the tool. If the environment
	// is entirely static, you can set this parameter to "false"
	tool->enableDynamicObjects(true);

	// haptic forces are enabled only if small forces are first sent to the device;
	// this mode avoids the force spike that occurs when the application starts when
	// the tool is located inside an object for instance.
	tool->setWaitForSmallForce(true);

	// start the haptic tool
	tool->start();

	// read the scale factor between the physical workspace of the haptic
	// device and the virtual workspace defined for the tool
	double workspaceScaleFactor = tool->getWorkspaceScaleFactor();

	// stiffness properties
	double maxLinearForce = info.m_maxLinearForce;
	double maxLinearDamping = info.m_maxLinearDamping;
	double maxStiffness = info.m_maxLinearStiffness / workspaceScaleFactor;

	//--------------------------------------------------------------------------
	// OBJECTS
	//--------------------------------------------------------------------------
	// create virtual objects
	refDice = new cMultiMesh();
	actDice = new cMultiMesh();
	boundingSphere = new cMesh();
	virtualButton = new cMesh();

	// assign name to the virtualButton object
	virtualButton->m_name = "virtualButton";
	actDice->m_name = "actDice";

	// add objects to the world
	world->addChild(refDice);
	world->addChild(actDice);
	actDice->addChild(boundingSphere);
	world->addChild(virtualButton);

	// position object
	actDice->setLocalPos(0.0, 1.0, 0.0);
	refDice->setLocalPos(0.0, -1.0, 0.0);
	boundingSphere->setLocalPos(0.0, 0.0, 0.0);
	virtualButton->setLocalPos(-0.5, 0.0, -1.0);
	
	// Orientation of the reference dice
	double angleX = rand() % 360;
	double angleY = rand() % 360;
	double angleZ = rand() % 360;
	refDice->rotateExtrinsicEulerAnglesDeg(angleX, angleY, angleZ, C_EULER_ORDER_XYZ);

	// Load object files
	/*actDice->loadFromFile("../../models/dice.obj");
	refDice->loadFromFile("../../models/dice.obj");*/
	actDice->loadFromFile("C:/Users/nm911876/Desktop/Projects/DiceGame/models/dice.obj");
	refDice->loadFromFile("C:/Users/nm911876/Desktop/Projects/DiceGame/models/dice.obj");

	// Radius of the bounding sphere for the actual dice (manipulated by the user)
	radii = cSub(actDice->getBoundaryMax(), actDice->getBoundaryMin()).length() * scale * 0.5;

	// create the bounding sphere
	cCreateSphere(boundingSphere, radii);
	// create the virtual button
	cCreateSphere(virtualButton, radii / 2);

	// adjust transparency levels
	boundingSphere->setTransparencyLevel(0.25);
	virtualButton->setTransparencyLevel(0.5);

	// create material
	cMaterial matMembrane;
	matMembrane.setStiffness(0.5 * maxStiffness);

	// Assign material
	actDice->setMaterial(matMembrane);
	refDice->setMaterial(matMembrane);
	virtualButton->setMaterial(matMembrane);

	// scale object
	actDice->scale(scale);
	refDice->scale(scale);

	// create collision detector
	actDice->createAABBCollisionDetector(toolRadius);
	virtualButton->createAABBCollisionDetector(toolRadius);

	boundingSphere->setEnabled(false);


    //--------------------------------------------------------------------------
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    cFont *font = NEW_CFONTCALIBRI20();
    
    // create a label to display the haptic rate of the simulation
    labelHapticRate = new cLabel(font);
    labelHapticRate->m_fontColor.setWhite();
    camera->m_frontLayer->addChild(labelHapticRate);

    //--------------------------------------------------------------------------
    // START SIMULATION
    //--------------------------------------------------------------------------

    // create a thread which starts the main haptics rendering loop
    cThread* hapticsThread = new cThread();

	// create threads for data logging and writing
	//cThread* dataThread = new cThread();
	cThread* flushingThread = new cThread();

	hapticsThread->start(updateHaptics, CTHREAD_PRIORITY_HAPTICS);
	//dataThread->start(logData, CTHREAD_PRIORITY_HAPTICS);
	flushingThread->start(flushData, CTHREAD_PRIORITY_GRAPHICS);

    // setup callback when application exits
    atexit(close);

    // start the main graphics rendering loop
    glutTimerFunc(50, graphicsTimer, 0);
    glutMainLoop();

    // exit
    return (0);
}

//------------------------------------------------------------------------------

void resizeWindow(int w, int h)
{
    windowW = w;
    windowH = h;
}

//------------------------------------------------------------------------------

void keySelect(unsigned char key, int x, int y)
{
    // option ESC: exit
    if ((key == 27) || (key == 'x'))
    {
        close();
        exit(0);
    }

	if (key == 32)
	{
		// Orientation of the reference dice
		double angleX = rand() % 360;
		double angleY = rand() % 360;
		double angleZ = rand() % 360;
		refDice->rotateExtrinsicEulerAnglesDeg(angleX, angleY, angleZ, C_EULER_ORDER_XYZ);
	}
}

//------------------------------------------------------------------------------

void mouseClick(int button, int state, int x, int y)
{
	if (button == GLUT_LEFT_BUTTON)
	{
		mouseLeftClick = (state == GLUT_DOWN);
		mouseX = x;
		mouseY = y;
	}	
}

//------------------------------------------------------------------------------

void mouseMove(int x, int y)
{
	// compute mouse motion
	int dx = x - mouseX;
	int dy = y - mouseY;
	mouseX = x;
	mouseY = y;
	
	// compute ne camera angles
	double azimuthDeg = camera->getSphericalAzimuthDeg() + (0.5 * dy);
	double polarDeg = camera->getSphericalPolarDeg() + (-0.5 * dx);

	if (mouseLeftClick)
	{
		// assign new angles
		camera->setSphericalAzimuthDeg(azimuthDeg);
		camera->setSphericalPolarDeg(polarDeg);

		// line up tool with camera
		tool->setLocalRot(camera->getLocalRot());
	}
}

//------------------------------------------------------------------------------

void close(void)
{
    // stop the simulation
    simulationRunning = false;

    // wait for graphics and haptics loops to terminate
    while (!simulationFinished) { cSleepMs(100); }

    // close haptic device
    hapticDevice->close();

	Sleep(100);

	// close data file
	dataBuffer.safe_flush(dataFile);
	Sleep(100);
	fclose(dataFile);
}

//------------------------------------------------------------------------------

void graphicsTimer(int data)
{
    if (simulationRunning)
    {
        glutPostRedisplay();
    }

    glutTimerFunc(50, graphicsTimer, 0);
}

//------------------------------------------------------------------------------

void updateGraphics(void)
{
    /////////////////////////////////////////////////////////////////////
    // UPDATE WIDGETS
    /////////////////////////////////////////////////////////////////////

    // display haptic rate data
    labelHapticRate->setText(cStr(frequencyCounter.getFrequency(), 0) + " Hz");

    // update position of label
    labelHapticRate->setLocalPos((int)(0.5 * (windowW - labelHapticRate->getWidth())), 15);


    /////////////////////////////////////////////////////////////////////
    // RENDER SCENE
    /////////////////////////////////////////////////////////////////////

    // update shadow maps (if any)
    world->updateShadowMaps(false, mirroredDisplay);

    // render world
    camera->renderView(windowW, windowH);

    // swap buffers
    glutSwapBuffers();

    // wait until all GL commands are completed
    glFinish();

    // check for any OpenGL errors
    GLenum err;
    err = glGetError();
    if (err != GL_NO_ERROR) cout << "Error:  %s\n" << gluErrorString(err);
}

//------------------------------------------------------------------------------

void updateHaptics(void)
{
	cMode state = IDLE;
	cVirtualMode vState = vmIDLE;
	cTransform tool_T_object;

	HapticData tmpData;

	// update state
	simulationRunning = true;
	simulationFinished = false;

	while (simulationRunning)
	{
		// update frequency counter
		frequencyCounter.signal(1);

		// compute global reference frames for each object
		world->computeGlobalPositions(true);

		// update position and orientation of tool
		tool->updateFromDevice();

		// compute interaction forces
		tool->computeInteractionForces();

		
		//-------------------------------------------------------------
		// Manipulation
		//-------------------------------------------------------------

		// compute transformation from world to tool (haptic device)
		cTransform world_T_tool = tool->getDeviceGlobalTransform();

		// get status of user switch
		bool robotButton1 = tool->getUserSwitch(0);

		//
		// STATE 1:
		// Idle mode - user presses the user switch
		//
		if ((state == IDLE) && (robotButton1 == true))
		{
			// Increas the number of clicks counter
			//m_numClicks++;

			// check if at least one contact has occurred
			if (tool->m_hapticPoint->getNumCollisionEvents() > 0)
			{
				// get contact event
				cCollisionEvent* collisionEvent = tool->m_hapticPoint->getCollisionEvent(0);

				// get object from contact event
				selectedObject = collisionEvent->m_object->getParent();
				if (selectedObject-> m_name == "actDice")
				{
					// get transformation from object
					cTransform world_T_object = selectedObject->getGlobalTransform();

					// compute inverse transformation from contact point to object
					cTransform tool_T_world = world_T_tool;
					tool_T_world.invert();

					// store current transformation tool
					tool_T_object = tool_T_world * world_T_object;

					// update state
					state = SELECTION;
				}
				else
					selectedObject = NULL;
			}
		}
		//
		// STATE 2:
		// Selection mode - operator maintains user switch enabled and moves object
		//
		else if ((state == SELECTION) && (robotButton1 == true))
		{
			// compute new transformation of object in global coordinates
			cTransform world_T_object = world_T_tool * tool_T_object;

			// compute new transformation of object in local coordinates
			cTransform parent_T_world = selectedObject->getParent()->getLocalTransform();
			parent_T_world.invert();
			cTransform parent_T_object = parent_T_world * world_T_object;

			// assign new local transformation to object
			selectedObject->setLocalTransform(parent_T_object);

			// set zero forces when manipulating objects
			tool->setDeviceGlobalForce(0.0, 0.0, 0.0);

			tool->initialize();
		}
		//
		// STATE 3:
		// Finalize Selection mode - operator releases user switch.
		//
		else
		{
			state = IDLE;
		}

		//-------------------------------------------------------------
		// Start/stop experiment
		//-------------------------------------------------------------
		cGenericObject* contactObject;
		if (tool->m_hapticPoint->getNumCollisionEvents() > 0 && vState == vmIDLE)
		{
			if (tool->m_hapticPoint->getCollisionEvent(0)->m_object->m_name == "virtualButton" && vState == vmIDLE)
			{
				timer.stop();
				vState = vmCONTACT;
			}
		}
		else if (tool->m_hapticPoint->getNumCollisionEvents() == 0 && vState == vmCONTACT)
		{
			double angleX = rand() % 360;
			double angleY = rand() % 360;
			double angleZ = rand() % 360;

			refDice->rotateExtrinsicEulerAnglesDeg(angleX, angleY, angleZ, C_EULER_ORDER_XYZ);

			resetWorld();

			// time measurement
			cout << "Elapsed time: " << timer.getCurrentTimeSeconds() << endl;
			timer.reset();
			Sleep(10);
			timer.start();

			// set the virtual button state to idle
			vState = vmIDLE;
		}
		
		// send forces to haptic device
		tool->applyToDevice();

		// Log data temporaryly to tmpData struct
		hapticDevice->getPosition(tmpData.devicePos);
		hapticDevice->getLinearVelocity(tmpData.deviceVel);
		hapticDevice->getRotation(tmpData.deviceOrientation);
		tmpData.actDicePos = actDice->getLocalPos();
		tmpData.actDiceOrientation = actDice->getLocalRot();
		tmpData.refDiceOrientation = refDice->getLocalRot();
		tmpData.time = timer.getCurrentTimeSeconds();

		dataBuffer.push_back(tmpData);
	}

	// disable forces
	hapticDevice->setForceAndTorqueAndGripperForce(cVector3d(0.0, 0.0, 0.0),
		cVector3d(0.0, 0.0, 0.0),
		0.0);

	// update state
	simulationRunning = false;
	simulationFinished = true;
}

//------------------------------------------------------------------------------

void createMenu(void)
{
	int menu;
	
	menu = glutCreateMenu(processMenuEvents);

	// Add entries to the menu
	glutAddMenuEntry("Show Bounding Sphere", BOUNDING_SPHERE);
	glutAddMenuEntry("---------------------", SEPARATOR);
	glutAddMenuEntry("Full Screen", FULL_SCREEN);
	glutAddMenuEntry("Mirror Display", MIRROR_DISPLAY);
	glutAddMenuEntry("---------------------", SEPARATOR);
	glutAddMenuEntry("Reset world", RESET_WORLD);
	glutAddMenuEntry("---------------------", SEPARATOR);
	glutAddMenuEntry("Exit", EXIT_APP);

	// Attache the menu to the right button
	glutAttachMenu(GLUT_RIGHT_BUTTON);
}

//------------------------------------------------------------------------------

void processMenuEvents(int option)
{
	switch (option)
	{
	case FULL_SCREEN:
		if (fullscreen)
		{
			windowPosX = glutGet(GLUT_INIT_WINDOW_X);
			windowPosY = glutGet(GLUT_INIT_WINDOW_Y);
			windowW = glutGet(GLUT_INIT_WINDOW_WIDTH);
			windowH = glutGet(GLUT_INIT_WINDOW_HEIGHT);
			glutPositionWindow(windowPosX, windowPosY);
			glutReshapeWindow(windowW, windowH);
			fullscreen = false;
		}
		else
		{
			glutFullScreen();
			fullscreen = true;
		}
		break;
	case EXIT_APP:
		close();
		exit(0);
		break;
	case MIRROR_DISPLAY:
		mirroredDisplay = !mirroredDisplay;
		camera->setMirrorVertical(mirroredDisplay);
		break;
	case BOUNDING_SPHERE:
		boundingSphere->setEnabled(!(boundingSphere->getEnabled()));
		break;
	case SEPARATOR:
		break;
	case RESET_WORLD:
		resetWorld();
	}
}

//------------------------------------------------------------------------------

void flushData(void)
{
	while (simulationRunning)
	{
		dataBuffer.safe_flush(dataFile);
	}

	dataBuffer.safe_flush(dataFile);

	// update state
	simulationRunning = false;
	simulationFinished = true;
}

//------------------------------------------------------------------------------

void resetWorld(void)
{
	actDice->setLocalPos(0.0, 1.0, 0.0);
	actDice->setLocalRot(cMatrix3d(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0));
}

//------------------------------------------------------------------------------
