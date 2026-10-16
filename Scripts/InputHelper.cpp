#include "InputHelper.h"

#include <stdio.h>
#include <Aurora/System/SystemManager.h>

InputHelper::InputHelper()
{

}

InputHelper::~InputHelper()
{

}

void InputHelper::Init()
{
	//move
	actionNames[0] = std::string("Forward");
	actionNames[1] = std::string("Back");
	actionNames[2] = std::string("Left");
	actionNames[3] = std::string("Right");

	//rotate
	actionNames[4] = std::string("Rotate up");
	actionNames[5] = std::string("Rotate down");
	actionNames[6] = std::string("Rotate left");
	actionNames[7] = std::string("Rotate right");

	//switch block
	actionNames[8] = std::string("Switch block left");
	actionNames[9] = std::string("Switch block right");
	actionNames[10] = std::string("Switch set up");
	actionNames[11] = std::string("Switch set down");

	actionNames[12] = std::string("Jump");
	actionNames[13] = std::string("Remove block");
	actionNames[14] = std::string("Place block");
	actionNames[15] = std::string("Menu");
	actionNames[16] = std::string("Chat");
	actionNames[17] = std::string("Sneak");

	//button names
	buttonNames[0] = std::string("Up");
	buttonNames[1] = std::string("Down");
	buttonNames[2] = std::string("Left");
	buttonNames[3] = std::string("Right");

	buttonNames[4] = std::string("Triangle");
	buttonNames[5] = std::string("Cross");
	buttonNames[6] = std::string("Square");
	buttonNames[7] = std::string("Circle");

	buttonNames[8] = std::string("Left Trigger");
	buttonNames[9] = std::string("Right Trigger");
	buttonNames[10] = std::string("Select");

	buttonNames[11] = std::string("Analog up");
	buttonNames[12] = std::string("Analog down");
	buttonNames[13] = std::string("Analog left");
	buttonNames[14] = std::string("Analog right");
	buttonNames[15] = std::string("Start");

	//connections
	connections[0] = InputConection(0,0,11);
	connections[1] = InputConection(1,1,12);
	connections[2] = InputConection(2,2,13);
	connections[3] = InputConection(3,3,14);
	connections[4] = InputConection(4,4,4);
	connections[5] = InputConection(5,5,5);
	connections[6] = InputConection(6,6,6);
	connections[7] = InputConection(7,7,7);
	connections[8] = InputConection(8,8,2);
	connections[9] = InputConection(9,9,3);
	connections[10] = InputConection(10,10,0);
	connections[11] = InputConection(11,11,1);
	connections[12] = InputConection(12,12,10);
	connections[13] = InputConection(13,13,8);
	connections[14] = InputConection(14,14,9);
	connections[15] = InputConection(15,15,15);
	connections[16] = InputConection(16,16,0);
	connections[17] = InputConection(17,17,1);

	//default analog sensitivity
	analogXright = analogYup = 0.4f;
	analogXleft = analogYdown = -0.4f;
}

std::string InputHelper::getActionName(int number)
{
	if(number < 0 || number >= ACTION_COUNT)
		return "";

	return actionNames[number];
}

std::string InputHelper::getButtonName(int number)
{
	if(number < 0 || number >= BUTTON_COUNT)
		return "";

	return buttonNames[number];
}

InputConection InputHelper::getConnection(int number)
{
	if(number < 0 || number >= ACTION_COUNT)
		return InputConection();

	return connections[number];
}

void InputHelper::setButtonToAction(int action,int button)
{
	if(action < 0 || action >= ACTION_COUNT)
		return;

	connections[action].button = button;
}

int InputHelper::getButtonToAction(int action)
{
	if(action < 0 || action >= ACTION_COUNT)
		return -1;

	return connections[action].button;
}

void InputHelper::Save()
{
	FILE * pFile;
	pFile = fopen("Config/input.cf","wb");

	if(pFile == NULL)
		return;

	for(int i = 0;i < ACTION_COUNT;i++)
	{
		fwrite(&connections[i].key,sizeof(int),1,pFile);
		fwrite(&connections[i].action,sizeof(int),1,pFile);
		fwrite(&connections[i].button,sizeof(int),1,pFile);
	}

	//analog
	fwrite(&analogYup,sizeof(float),1,pFile);
	fwrite(&analogYdown,sizeof(float),1,pFile);
	fwrite(&analogXleft,sizeof(float),1,pFile);
	fwrite(&analogXright,sizeof(float),1,pFile);

	// Analog stick calibration. A fixed-size trailing block, and it must stay fixed-size or the length
	// test Load makes would be lying. The values themselves live in SystemManager, because that is what
	// applies them; this file owns the input configuration and so owns the file they are stored in.
	bool calibrated = Aurora::System::SystemManager::Instance()->IsAnalogCalibrated();
	float baseX = 0.0f, baseY = 0.0f, rangeX = 1.0f, rangeY = 1.0f;
	if(calibrated)
	{
		Aurora::System::SystemManager::Instance()->GetAnalogCalibration(baseX, baseY, rangeX, rangeY);
	}
	fwrite(&calibrated,sizeof(bool),1,pFile);
	fwrite(&baseX,sizeof(float),1,pFile);
	fwrite(&baseY,sizeof(float),1,pFile);
	fwrite(&rangeX,sizeof(float),1,pFile);
	fwrite(&rangeY,sizeof(float),1,pFile);

	//close file
	fclose(pFile);
}

void InputHelper::Load()
{
	FILE * pFile;
	pFile = fopen("Config/input.cf","rb");

	if(pFile == NULL)
		return;

	fseek(pFile, 0, SEEK_END);
	long size = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	const long analogBytes = (long)(4 * sizeof(float));
	const long newSize = (long)(ACTION_COUNT * 3 * sizeof(int)) + analogBytes;
	const long size17 = (long)(17 * 3 * sizeof(int)) + analogBytes;
	const long size16 = (long)(16 * 3 * sizeof(int)) + analogBytes;

	int count = ACTION_COUNT;
	if(size < newSize)
	{
		if(size >= size17)
			count = 17;
		else if(size >= size16)
			count = 16;
		else
		{
			fclose(pFile);
			return;
		}
	}

	for(int i = 0;i < count;i++)
	{
		InputConection connection;

		fread(&connection.key,sizeof(int),1,pFile);
		fread(&connection.action,sizeof(int),1,pFile);
		fread(&connection.button,sizeof(int),1,pFile);

		connections[i] = connection;
	}

	//analog
	fread(&analogYup,sizeof(float),1,pFile);
	fread(&analogYdown,sizeof(float),1,pFile);
	fread(&analogXleft,sizeof(float),1,pFile);
	fread(&analogXright,sizeof(float),1,pFile);

	// Analog stick calibration, which only a file written since it was added carries. The length is the
	// version number -- there is no byte in the format that could hold one without breaking every
	// Config/input.cf already on a card, and the length test above is already how the 16- and
	// 17-action formats are told apart, so this is the same trick applied once more.
	const long calBytes = (long)(sizeof(bool) + 4 * sizeof(float));
	if(size >= newSize + calBytes)
	{
		bool calibrated = false;
		float baseX = 0.0f, baseY = 0.0f, rangeX = 1.0f, rangeY = 1.0f;
		fread(&calibrated,sizeof(bool),1,pFile);
		fread(&baseX,sizeof(float),1,pFile);
		fread(&baseY,sizeof(float),1,pFile);
		fread(&rangeX,sizeof(float),1,pFile);
		fread(&rangeY,sizeof(float),1,pFile);
		if(calibrated)
		{
			Aurora::System::SystemManager::Instance()->SetAnalogCalibration(baseX, baseY, rangeX, rangeY);
		}
	}

	//close file
	fclose(pFile);
}

InputHelper InputHelper::m_InputHelper;

InputHelper* InputHelper::Instance()
{
	return &m_InputHelper;
}
