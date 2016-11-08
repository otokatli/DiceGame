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
- Multi-finger support

*/
//==============================================================================

//------------------------------------------------------------------------------
#include "chai3d.h"
#include "block_linked_list.h"
#include "ConfFile.h"
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
//------------------------------------------------------------------------------
#include "CODE.h"
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

// simulation clock
cPrecisionClock simClock;

// index of the current subexperiment
int indSubExp = 0;

// configuration file for the experiment
ConfFile config;

// torque gain
double torqueGain = 2.0;

//---------------------------------------------------------------------------
// ODE MODULE VARIABLES
//---------------------------------------------------------------------------

// ODE world
cODEWorld* ODEWorld;

// ODE objects
cODEGenericBody* ODEactDice;

// ODE objects
cODEGenericBody* ODEGPlane0;
cODEGenericBody* ODEGPlane1;
cODEGenericBody* ODEGPlane2;
cODEGenericBody* ODEGPlane3;
cODEGenericBody* ODEGPlane4;
cODEGenericBody* ODEGPlane5;

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

	//cout << "Name of the student/participant id:";
	//cin.get(userName, 256);
	//cout << endl << endl;

	//--------------------------------------------------------------------------
	// OPEN CONFIGURATION FILE
	//--------------------------------------------------------------------------
	config.openConfFile("C:/Users/nm911876/Desktop/Projects/DiceGame/bin/win-x64/experiment.conf");
	cout << config.m_numSubExp << " configuration(s) is/are loaded." << endl;
	//config.printConfigurations();

	//--------------------------------------------------------------------------
	// OPEN FILE FOR DATA RECORDING
	//--------------------------------------------------------------------------
	dataFile = fopen("data.hdata", "wb");
	if (dataFile == 0)
	{
		cerr << "Error: Output data file could not be opened!";
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
		30,      // spherical coordinate azimuth angle
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
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    cFont *font = NEW_CFONTCALIBRI20();
    
    // create a label to display the haptic rate of the simulation
    labelHapticRate = new cLabel(font);
    labelHapticRate->m_fontColor.setWhite();
    camera->m_frontLayer->addChild(labelHapticRate);

	//--------------------------------------------------------------------------
	// CREATE ODE WORLD AND OBJECTS
	//--------------------------------------------------------------------------
	refDice = new cMultiMesh();
	boundingSphere = new cMesh();
	virtualButton = new cMesh();

	// assign name to the virtualButton object
	virtualButton->m_name = "virtualButton";

	// add objects to the world
	world->addChild(refDice);
	world->addChild(virtualButton);

	// position object
	refDice->setLocalPos(0.0, -1.0, 0.0);
	boundingSphere->setLocalPos(0.0, 0.0, 0.0);
	virtualButton->setLocalPos(-0.5, -1.0, 1.0);

	// Load object files
	refDice->loadFromFile("C:/Users/nm911876/Desktop/Projects/DiceGame/models/dice.obj");

	// Radius of the bounding sphere for the actual dice (manipulated by the user)
	radii = cSub(refDice->getBoundaryMax(), refDice->getBoundaryMin()).length() * scale * 0.5;

	// create the bounding sphere
	cCreateSphere(boundingSphere, radii);
	// create the virtual button
	cCreateSphere(virtualButton, radii / 2);

	// adjust transparency levels
	boundingSphere->setTransparencyLevel(0.25);
	//virtualButton->setTransparencyLevel(0.75);

	// create material
	cMaterial matMembrane;
	cMaterial matButton;
	matMembrane.setStiffness(0.5 * maxStiffness);
	matButton.setStiffness(0.5 * maxStiffness);
	matButton.setBlueCadet();

	// Assign material
	refDice->setMaterial(matMembrane);
	virtualButton->setMaterial(matButton);

	// scale object
	refDice->scale(scale);

	// create collision detector
	virtualButton->createAABBCollisionDetector(toolRadius);

	boundingSphere->setEnabled(false);

	//--------------------------------------------------------------------------
	// CREATE ODE WORLD AND OBJECTS
	//--------------------------------------------------------------------------
	//////////////////////////////////////////////////////////////////////////
	// ODE WORLD
	//////////////////////////////////////////////////////////////////////////
	
	// create an ODE world to simulate dynamic bodies
	ODEWorld = new cODEWorld(world);

	// add ODE world as a node inside world
	world->addChild(ODEWorld);

	// set some gravity
	//ODEWorld->setGravity(cVector3d(0.00, 0.00, -9.81));
	ODEWorld->setGravity(cVector3d(0.00, 0.00, 0.0));

	// define damping properties
	ODEWorld->setAngularDamping(0.00002);
	ODEWorld->setLinearDamping(0.00002);

	//////////////////////////////////////////////////////////////////////////
	// DICE OBJECT
	//////////////////////////////////////////////////////////////////////////
	ODEactDice = new cODEGenericBody(ODEWorld);

	cMultiMesh* actDice = new cMultiMesh();
	actDice->setLocalPos(0.0, 1.0, 0.0);
	actDice->loadFromFile("C:/Users/nm911876/Desktop/Projects/DiceGame/models/dice.obj");
	// Radius of the bounding sphere for the actual dice (manipulated by the user)
	radii = cSub(actDice->getBoundaryMax(), actDice->getBoundaryMin()).length() * scale * 0.5;

	// assign name to the virtualButton object
	actDice->m_name = "actDice";

	// Assign material
	actDice->setMaterial(matMembrane);

	// create collision detector
	actDice->createAABBCollisionDetector(toolRadius);

	// add mesh to ODE object
	ODEactDice->setImageModel(actDice);

	// create a dynamic model of the ODE object. Here we decide to use a box just like
	// the object mesh we just defined
	ODEactDice->createDynamicBox(radii, radii, radii);

	// define some mass properties for each cube
	ODEactDice->setMass(0.05);

	// set position of the dice
	ODEactDice->setLocalPos(0.0, 1.0, 0.0);

	//////////////////////////////////////////////////////////////////////////
	// 6 ODE INVISIBLE WALLS
	//////////////////////////////////////////////////////////////////////////

	// we create 6 static walls to contains the 3 cubes within a limited workspace
	ODEGPlane0 = new cODEGenericBody(ODEWorld);
	ODEGPlane1 = new cODEGenericBody(ODEWorld);
	ODEGPlane2 = new cODEGenericBody(ODEWorld);
	ODEGPlane3 = new cODEGenericBody(ODEWorld);
	ODEGPlane4 = new cODEGenericBody(ODEWorld);
	ODEGPlane5 = new cODEGenericBody(ODEWorld);

	double width = 1.0;
	ODEGPlane0->createStaticPlane(cVector3d(0.0, 0.0, 2.0 *width), cVector3d(0.0, 0.0, -1.0));
	ODEGPlane1->createStaticPlane(cVector3d(0.0, 0.0, -width), cVector3d(0.0, 0.0, 1.0));
	ODEGPlane2->createStaticPlane(cVector3d(0.0, width, 0.0), cVector3d(0.0, -1.0, 0.0));
	ODEGPlane3->createStaticPlane(cVector3d(0.0, -width, 0.0), cVector3d(0.0, 1.0, 0.0));
	ODEGPlane4->createStaticPlane(cVector3d(width, 0.0, 0.0), cVector3d(-1.0, 0.0, 0.0));
	ODEGPlane5->createStaticPlane(cVector3d(-0.8 * width, 0.0, 0.0), cVector3d(1.0, 0.0, 0.0));


	//////////////////////////////////////////////////////////////////////////
	// GROUND
	//////////////////////////////////////////////////////////////////////////

	// create a mesh that represents the ground
	cMesh* ground = new cMesh();
	ODEWorld->addChild(ground);

	// create a plane
	double groundSize = 3.0;
	cCreatePlane(ground, groundSize, groundSize);

	// position ground in world where the invisible ODE plane is located (ODEGPlane1)
	ground->setLocalPos(0.0, 0.0, -1.0);

	// define some material properties and apply to mesh
	cMaterial matGround;
	matGround.setStiffness(0.3 * maxStiffness);
	matGround.setDynamicFriction(0.2);
	matGround.setStaticFriction(0.0);
	matGround.setWhite();
	matGround.m_emission.setGrayLevel(0.3);
	ground->setMaterial(matGround);
	ground->setTransparencyLevel(1.0);

	// setup collision detector
	ground->createAABBCollisionDetector(toolRadius);

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
	// update state
	simulationRunning = true;
	simulationFinished = false;

	while (simulationRunning)
	{
		// compute global reference frames for each object
		world->computeGlobalPositions(true);

		// update positions
		tool->updateFromDevice();

		// compute interaction forces
		tool->computeInteractionForces();

		// apply forces to haptic device
		tool->setDeviceGlobalTorque(torqueGain * tool->getDeviceGlobalTorque());
		tool->applyToDevice();

		// apply forces to ODE object
		int numInteractionPoints = tool->getNumHapticPoints();
		for (int j = 0; j < numInteractionPoints; ++j)
		{
			// get pointer to next interaction point of tool
			cHapticPoint* interactionPoint = tool->getHapticPoint(j);

			// check all contact points
			int numContacts = interactionPoint->getNumCollisionEvents();
			for (int k = 0; k < numContacts; k++)
			{
				cCollisionEvent* collisionEvent = interactionPoint->getCollisionEvent(k);

				// given the mesh object we may be touching, we search for its owner which
				// could be the mesh itself or a multi-mesh object. Once the owner found, we
				// look for the parent that will point to the ODE object itself.
				cGenericObject* object = collisionEvent->m_object->getOwner()->getOwner();

				// cast to ODE object
				cODEGenericBody* ODEobject = dynamic_cast<cODEGenericBody*>(object);

				// if ODE object, we apply interaction forces
				if (ODEobject != NULL)
				{
					ODEobject->addExternalForceAtPoint(-0.3 * interactionPoint->getLastComputedForce(),
						collisionEvent->m_globalPos);
				}
			}
		}

		// retrieve simulation time and compute next interval
		double time = simClock.getCurrentTimeSeconds();
		double nextSimInterval = cClamp(time, 0.0001, 0.001);

		// reset clock
		simClock.reset();
		simClock.start();

		// update simulation
		ODEWorld->updateDynamics(nextSimInterval);
	}

	// disable forces
	hapticDevice->setForceAndTorqueAndGripperForce(cVector3d(0.0, 0.0, 0.0), cVector3d(0.0, 0.0, 0.0), 0.0);

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
	camera->setSphericalDeg(4.0,    // spherical coordinate radius
		30,      // spherical coordinate azimuth angle
		0);     // spherical coordinate polar angle

	// line up tool with camera
	tool->setLocalRot(camera->getLocalRot());

	ODEactDice->setLocalPos(0.0, 1.0, 0.0);
	ODEactDice->setLocalRot(cMatrix3d(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0));
}

//------------------------------------------------------------------------------
