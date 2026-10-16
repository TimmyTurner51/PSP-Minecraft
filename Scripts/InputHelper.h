#ifndef INPUTHELPER_H_
#define INPUTHELPER_H_

#include <string>

class InputConection
{
public:
	int key;
	int action;
	int button;

	InputConection()
	{
		key = 0;
		action = 0;
		button = 0;
	}

	InputConection(int k,int a,int b)
	{
		key = k;
		action = a;
		button = b;
	}
};

class InputHelper
{
public:
	static const int ACTION_COUNT = 18;
	static const int ACTION_CHAT = 16;
	static const int ACTION_SNEAK = 17;
	static const int BUTTON_COUNT = 16;

	InputHelper();
	~InputHelper();

	static InputHelper m_InputHelper;
	static InputHelper* Instance();

	void Init();

	std::string getActionName(int number);
	std::string getButtonName(int number);
	InputConection getConnection(int number);

	void setButtonToAction(int action,int button);
	int getButtonToAction(int action);

	float analogXleft,analogXright;
	float analogYup,analogYdown;

	void Save();
	void Load();

private:

	std::string actionNames[ACTION_COUNT];
	std::string buttonNames[BUTTON_COUNT];
	InputConection connections[ACTION_COUNT];
};

#endif
