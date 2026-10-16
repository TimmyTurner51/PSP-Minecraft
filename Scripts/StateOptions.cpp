#include "StateOptions.h"
#include "InputHelper.h"
#include "TextureHelper.h"

// font vars
#define default_size 0.5
#define default_big_size 0.687

#define ENGLISH 1
#define RUSSIAN 2

// Analog stick calibration, in the order the screen walks through them. Two measurement phases and not
// one, because the two readings contradict each other: the resting offset has to be taken while the
// stick is untouched, and the travel has to be taken while it is being pushed as far as it will go.
static const int kCalRestIdle   = 0;   // waiting for the stick to be released
static const int kCalRestSample = 1;   // averaging the resting reading
static const int kCalRoll       = 2;   // tracking the sweep to the rim
static const int kCalDone       = 3;   // calibrated, waiting to save

// Frames the resting reading is averaged over: a third of a second, because the pad's low byte jitters
// and a single sample would make the baseline depend on which frame it happened to land on.
static const int   kCalSettleFrames = 20;
// The furthest from centre a *resting* stick may read before the reading is refused. A stick held while
// it is measured produces a baseline wrong by however far it was held, and nothing in the numbers says
// so, which is why this is checked rather than assumed: the player gets told to let go.
static const float kCalRestLimit    = 0.6f;
// The least travel an axis may show and still count as a sweep, so a calibration cannot be "completed"
// by pressing X twice without touching the stick. The same floor SetAnalogCalibration enforces; it is
// repeated here only so the screen can say why it refused instead of silently doing nothing.
static const float kCalMinRange     = 0.25f;

StateOptions::StateOptions()
{

}

StateOptions::~StateOptions()
{

}

void StateOptions::Init()
{
	//set render manager instance
	mRender = RenderManager::InstancePtr();
	mSystemMgr = SystemManager::Instance();
	mSoundMgr = SoundManager::Instance();

	buttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,0,95,12);
    buttonSprite->SetPosition(240,150);
    buttonSprite->Scale(2,2);

    nbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,24,95,12);
    nbuttonSprite->SetPosition(240,150);
    nbuttonSprite->Scale(2,2);

	sbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,12,95,12);
    sbuttonSprite->SetPosition(240,150);
    sbuttonSprite->Scale(2,2);

	halfbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,0,95,12);
	halfbuttonSprite->Scale(1.5,2);
	halfbuttonSprite->SetPosition(240,150);

	halfsbuttonSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,12,95,12);
	halfsbuttonSprite->Scale(1.5,2);
	halfsbuttonSprite->SetPosition(240,150);

	backSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Dirt),0,0,32,32);
	backSprite->Scale(2,2);

	moverSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,60,6,12);
	moverSprite->Scale(2,2);
	smoverSprite = new Sprite(TextureHelper::Instance()->GetTexture(TextureHelper::Buttons),0,60,6,12);
	smoverSprite->Scale(2,2);

	selectPos = 0;
	menuState = 0;

	//controls
	controlPos = 0;
	controlStart = 0;
	controlEnd = 0;

	//key states
	chooseKeyState = false;
	configChanged = false;
	currentKey = -1;
	newKey = -1;

	currentAnalogPos = 0;

	//analog stick calibration
	calState = kCalRestIdle;
	calSamples = 0;
	calFailed = false;
	calSumX = 0.0f;
	calSumY = 0.0f;
	calMinX = 0.0f;
	calMinY = 0.0f;
	calMaxX = 0.0f;
	calMaxY = 0.0f;
	calBaseX = 0.0f;
	calBaseY = 0.0f;
}

void StateOptions::Enter()
{
	mRender->SetOrtho(0,0,0,0,0,0);
	selectPos = 0;
}

void StateOptions::CleanUp()
{
	delete buttonSprite;
    delete nbuttonSprite;
	delete sbuttonSprite;
	delete halfbuttonSprite;
	delete halfsbuttonSprite;
	delete backSprite;
	delete moverSprite;
	delete smoverSprite;
}

void StateOptions::Pause()
{

}

void StateOptions::Resume()
{

	menuState = 0;
	mRender->SetOrtho(0,0,0,0,0,0);
}

void StateOptions::HandleEvents(StateManager* sManager)
{
	//update input
	mSystemMgr->InputUpdate();

	switch(menuState)
	{
		case 0:
		{
			//up, down
			if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
			{
				selectPos--;
				if(selectPos < 0)
					selectPos = 3;

				mSoundMgr->PlayMenuSound();
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
			{
				selectPos++;
				if(selectPos > 2)
					selectPos = 0;

				mSoundMgr->PlayMenuSound();
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
			{
				InputHelper::Instance()->Save();
				sManager->PopState();
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
			{
				if(selectPos == 0)
				{
					menuState = 1;

					controlPos = 0;
					controlStart = 0;
					controlEnd = 6;

					//reset key state settings
					chooseKeyState = false;
					configChanged = false;
					currentKey = -1;
					newKey = -1;

				}
				if(selectPos == 1)
				{
					menuState = 2;
					currentAnalogPos = 0;
				}
				// The Update entry that used to be here is gone, and its absence is the point of the
				// Launcher rather than a lost feature. A game cannot replace the binary it is running,
				// so updating from in here always needed a second program, a handoff file and a
				// one-way launch that could not come back. The Launcher owns the install instead and
				// can swap the game binary while nothing is running it. Cancel moves up into the slot
				// the entry occupied, so the menu stays three buttons rather than three and a gap.
				if(selectPos == 2)
				{
					InputHelper::Instance()->Save();
					sManager->PopState();
				}
			}
		}
		break;
		case 1:
		{
			if(chooseKeyState)//here we set new key for action
			{
				//check each button
				if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
					currentKey = 0;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
					currentKey = 1;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_LEFT))
					currentKey = 2;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
					currentKey = 3;//cross

				if(mSystemMgr->KeyPressed(PSP_CTRL_TRIANGLE))
					currentKey = 4;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
					currentKey = 5;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_SQUARE))
					currentKey = 6;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
					currentKey = 7;//cross

				if(mSystemMgr->KeyPressed(PSP_CTRL_LTRIGGER))
					currentKey = 8;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_RTRIGGER))
					currentKey = 9;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_SELECT))
					currentKey = 10;//cross
				if(mSystemMgr->KeyPressed(PSP_CTRL_START))
					currentKey = 15;//cross

				if(mSystemMgr->GetAnalogY() > InputHelper::Instance()->analogYup)//analog up
					currentKey = 12;//cross
				if(mSystemMgr->GetAnalogY() < InputHelper::Instance()->analogYdown)//analog down
					currentKey = 11;//cross
				if(mSystemMgr->GetAnalogX() < InputHelper::Instance()->analogXleft)//analog left
					currentKey = 13;//cross
				if(mSystemMgr->GetAnalogX() > InputHelper::Instance()->analogXright)//analog right
					currentKey = 14;//cross

				if(currentKey != -1)
				{
					//chek if it the same key
					if(InputHelper::Instance()->getButtonToAction(controlPos) != currentKey)
						configChanged = true;

					//set key
					InputHelper::Instance()->setButtonToAction(controlPos,currentKey);

					//quit
					chooseKeyState = false;

					mSoundMgr->PlayMenuSound();
				}

			}else
			{
				if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
				{
					controlPos--;

					if(controlPos < controlStart)
					{
						controlStart--;
						controlEnd--;

						if(controlEnd < 6)
						{
							controlStart = 0;
							controlEnd = 6;
						}
					}

					if(controlPos < 0)
					{
						controlPos = InputHelper::ACTION_COUNT;
						controlEnd = InputHelper::ACTION_COUNT;
						controlStart = InputHelper::ACTION_COUNT - 6;
						if(controlStart < 0)
							controlStart = 0;
					}

					mSoundMgr->PlayMenuSound();
				}

				if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
				{
					controlPos++;

					if(controlPos == controlEnd)
					{
						controlStart++;
						controlEnd++;
						if(controlEnd > InputHelper::ACTION_COUNT)
						{
							controlStart = InputHelper::ACTION_COUNT - 6;
							controlEnd = InputHelper::ACTION_COUNT;
						}
					}

					if(controlPos > InputHelper::ACTION_COUNT)
					{
						controlPos = 0;
						controlStart = 0;
						controlEnd = 6;
					}

					mSoundMgr->PlayMenuSound();
				}

				if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
				{
					menuState = 0;
				}

				if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
				{
					if(controlPos == InputHelper::ACTION_COUNT)//back button - exit from this menu
					{

						menuState = 0;
					}else//start key choosing
					{
						currentKey = -1;
						chooseKeyState = true;
					}
				}
			}
		}
		break;
		case 2:
		{
			if(mSystemMgr->KeyPressed(PSP_CTRL_UP))
			{
				currentAnalogPos--;

				if(currentAnalogPos < 0)
					currentAnalogPos = 5;

				mSoundMgr->PlayMenuSound();
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_DOWN))
			{
				currentAnalogPos++;
				if(currentAnalogPos > 5)
					currentAnalogPos = 0;

				mSoundMgr->PlayMenuSound();
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_LEFT))
			{
				//dodatnie
				if(currentAnalogPos == 0 || currentAnalogPos == 3)
				{
					if(currentAnalogPos == 0)
					{
						InputHelper::Instance()->analogYup -= 0.1f;
						if(InputHelper::Instance()->analogYup < 0.1f)
							InputHelper::Instance()->analogYup = 0.1f;
					}else
					{
						InputHelper::Instance()->analogXright -= 0.1f;
						if(InputHelper::Instance()->analogXright < 0.1f)
							InputHelper::Instance()->analogXright = 0.1f;
					}
				}
				//ujemne
				if(currentAnalogPos == 1 || currentAnalogPos == 2)
				{
					if(currentAnalogPos == 1)
					{
						InputHelper::Instance()->analogYdown += 0.1f;
						if(InputHelper::Instance()->analogYdown > -0.1f)
							InputHelper::Instance()->analogYdown = -0.1f;
					}else
					{
						InputHelper::Instance()->analogXleft += 0.1f;
						if(InputHelper::Instance()->analogXleft > -0.1f)
							InputHelper::Instance()->analogXleft = -0.1f;
					}
				}

				mSoundMgr->PlayMenuSound();
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_RIGHT))
			{
				//dodatnie
				if(currentAnalogPos == 0 || currentAnalogPos == 3)
				{
					if(currentAnalogPos == 0)
					{
						InputHelper::Instance()->analogYup += 0.1f;
						if(InputHelper::Instance()->analogYup > 0.9f)
							InputHelper::Instance()->analogYup = 0.9f;
					}else
					{
						InputHelper::Instance()->analogXright += 0.1f;
						if(InputHelper::Instance()->analogXright > 0.9f)
							InputHelper::Instance()->analogXright = 0.9f;
					}
				}
				//ujemne
				if(currentAnalogPos == 1 || currentAnalogPos == 2)
				{
					if(currentAnalogPos == 1)
					{
						InputHelper::Instance()->analogYdown -= 0.1f;
						if(InputHelper::Instance()->analogYdown < -0.9f)
							InputHelper::Instance()->analogYdown = -0.9f;
					}else
					{
						InputHelper::Instance()->analogXleft -= 0.1f;
						if(InputHelper::Instance()->analogXleft < -0.9f)
							InputHelper::Instance()->analogXleft = -0.9f;
					}
				}

				mSoundMgr->PlayMenuSound();
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
			{
				menuState = 0;
			}

			if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
			{
				if(currentAnalogPos == 4)
				{
					// Enter calibration. Reset here rather than in Init so re-entering the screen always
					// starts from "waiting for the stick", never from a half-finished sweep.
					calState = kCalRestIdle;
					calSamples = 0;
					calFailed = false;
					calSumX = 0.0f;
					calSumY = 0.0f;
					menuState = 3;
				}
				if(currentAnalogPos == 5)
					menuState = 0;
			}
		}
		break;
		case 3://analog stick calibration
		{
			switch(calState)
			{
				case kCalRestIdle:
				{
					if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
					{
						calSamples = 0;
						calSumX = 0.0f;
						calSumY = 0.0f;
						calFailed = false;
						calState = kCalRestSample;
						mSoundMgr->PlayMenuSound();
					}
					break;
				}
				case kCalRestSample:
				{
					// Averaged rather than sampled once: the pad's low byte jitters frame to frame, so a
					// single reading would make the baseline depend on which frame the button landed on.
					calSumX += mSystemMgr->GetAnalogRawX();
					calSumY += mSystemMgr->GetAnalogRawY();
					calSamples++;

					if(calSamples >= kCalSettleFrames)
					{
						float baseX = calSumX / (float)calSamples;
						float baseY = calSumY / (float)calSamples;

						// A stick that is still being held cannot be measured, and nothing in the numbers
						// says so -- it just produces a baseline wrong by however far it was held. So the
						// reading is checked against the nominal centre and refused by name instead.
						if(baseX > kCalRestLimit || baseX < -kCalRestLimit ||
						   baseY > kCalRestLimit || baseY < -kCalRestLimit)
						{
							calFailed = true;
							calState = kCalRestIdle;
							mSoundMgr->PlayMenuSound();
							break;
						}

						calBaseX = baseX;
						calBaseY = baseY;
						calMinX = baseX;
						calMaxX = baseX;
						calMinY = baseY;
						calMaxY = baseY;
						calState = kCalRoll;
					}
					break;
				}
				case kCalRoll:
				{
					float rawX = mSystemMgr->GetAnalogRawX();
					float rawY = mSystemMgr->GetAnalogRawY();
					if(rawX < calMinX) calMinX = rawX;
					if(rawX > calMaxX) calMaxX = rawX;
					if(rawY < calMinY) calMinY = rawY;
					if(rawY > calMaxY) calMaxY = rawY;

					if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS))
					{
						// Per axis, the larger of the two excursions from the base. The correction has to
						// be one number per axis, and a player who has rolled the stick properly went
						// about as far each way, so taking the larger side leaves the smaller one
						// slightly conservative rather than clipping it short.
						float rangeX = calMaxX - calBaseX;
						float negX = calBaseX - calMinX;
						if(negX > rangeX)
							rangeX = negX;

						float rangeY = calMaxY - calBaseY;
						float negY = calBaseY - calMinY;
						if(negY > rangeY)
							rangeY = negY;

						if(rangeX < kCalMinRange || rangeY < kCalMinRange)
						{
							calFailed = true;
							calState = kCalRestIdle;
							mSoundMgr->PlayMenuSound();
							break;
						}

						mSystemMgr->SetAnalogCalibration(calBaseX, calBaseY, rangeX, rangeY);
						calState = kCalDone;
						mSoundMgr->PlayMenuSound();
					}
					break;
				}
			}

			if(calState == kCalDone)
			{
				// Either button leaves once it is applied, and both save. The calibration is live the
				// moment SetAnalogCalibration returns, so leaving without writing it would quietly lose
				// it on the next boot -- a trap that is invisible until the drift comes back.
				if(mSystemMgr->KeyPressed(PSP_CTRL_CROSS) || mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
				{
					InputHelper::Instance()->Save();
					mSoundMgr->PlayMenuSound();
					menuState = 2;
				}
			}
			else if(mSystemMgr->KeyPressed(PSP_CTRL_CIRCLE))
			{
				// Abandoning before the sweep completes is clean: nothing has been applied yet, so the
				// calibration already in force is still the one that was there when the screen opened.
				menuState = 2;
			}
		}
		break;
	}
}

void StateOptions::Update(StateManager* sManager)
{

}

void StateOptions::Draw(StateManager* sManager)
{
	//start rendering
	mRender->StartFrame(1,1,1);

	switch(menuState)
	{
		case 0:
		{
			sceGuDisable(GU_DEPTH_TEST);
			sceGuEnable(GU_BLEND);
			sceGuColor(GU_COLOR(1,1,1,1.0f));

            for(int x = 0; x < 8; x++)
            {
                for(int y = 0; y < 5; y++)
                {
                    backSprite->SetPosition(x*64,y*64);
                    backSprite->Draw();
                }
            }


			//Controls
			buttonSprite->SetPosition(240,120);
			buttonSprite->Draw();

			//sensity - analog stick
			buttonSprite->SetPosition(240,150);
			buttonSprite->Draw();

			//cancel
			buttonSprite->SetPosition(240,180);
			buttonSprite->Draw();


			//selected button
			sbuttonSprite->SetPosition(240,(selectPos * 30) + 120);
			sbuttonSprite->Draw();

			sceGuDisable(GU_BLEND);
			sceGuEnable(GU_DEPTH_TEST);

			//draw subtitles on buttons
            if(mRender->GetFontLanguage() == ENGLISH)
            {
                selectPos == 0 ? DrawText(240,129,GU_COLOR(1,1,0,1) ,default_size,"Controls") : DrawText(240,129,GU_COLOR(1,1,1,1) ,default_size,"Controls");
                selectPos == 1 ? DrawText(240,159,GU_COLOR(1,1,0,1) ,default_size,"Analog stick") : DrawText(240,159,GU_COLOR(1,1,1,1) ,default_size,"Analog stick");
                selectPos == 2 ? DrawText(240,189,GU_COLOR(1,1,0,1) ,default_size,"Cancel") : DrawText(240,189,GU_COLOR(1,1,1,1) ,default_size,"Cancel");

                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Options");
            }
            if(mRender->GetFontLanguage() == RUSSIAN)
            {
                selectPos == 0 ? DrawText(240,129,GU_COLOR(1,1,0,1) ,default_size,"Naznayenie knopok") : DrawText(240,129,GU_COLOR(1,1,1,1) ,default_size,"Naznayenie knopok");
                selectPos == 1 ? DrawText(240,159,GU_COLOR(1,1,0,1) ,default_size,"Nastro~ki stika") : DrawText(240,159,GU_COLOR(1,1,1,1) ,default_size,"Nastro~ki stika");
                selectPos == 2 ? DrawText(240,189,GU_COLOR(1,1,0,1) ,default_size,"Otmena") : DrawText(240,189,GU_COLOR(1,1,1,1) ,default_size,"Otmena");

                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Nastro~ki");
            }
		}
		break;
		case 1://controls
		{
		    mRender->SetFont(ENGLISH);
			sceGuDisable(GU_DEPTH_TEST);
			sceGuEnable(GU_BLEND);
			sceGuColor(GU_COLOR(1,1,1,1.0f));

            for(int x = 0; x < 8; x++)
            {
                for(int y = 0; y < 5; y++)
                {
                    backSprite->SetPosition(x*64,y*64);
                    backSprite->Draw();
                }
            }


			int starty = 60;
			//something
			for(int i = controlStart;i < controlEnd;i++)
			{
				if(controlPos == i)
				{
					halfsbuttonSprite->SetPosition(160,starty + (i * 30) - (controlStart * 30));
					halfsbuttonSprite->Draw();
				}else
				{
					halfbuttonSprite->SetPosition(160,starty + (i * 30) - (controlStart * 30));
					halfbuttonSprite->Draw();
				}
			}

			//back
			if(controlPos == InputHelper::ACTION_COUNT)
			{
				sbuttonSprite->SetPosition(240,260);
				sbuttonSprite->Draw();
			}else
			{
				buttonSprite->SetPosition(240,260);
				buttonSprite->Draw();
			}

			sceGuDisable(GU_BLEND);
			sceGuEnable(GU_DEPTH_TEST);

			//write action names
			starty = 67;
			mRender->SetFontStyle(default_size,0xFFFFFFFF,0,0x00000000);
			for(int i = controlStart;i < controlEnd;i++)
			{
				//action
				controlPos == i ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000000) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000000);
				mRender->DebugPrint(250,starty + (i * 30) - (controlStart * 30)+4,InputHelper::Instance()->getActionName(i).c_str());
			}

			mRender->SetFontStyle(default_big_size,0xFFFFFFFF,0,0x00000200);
			starty = 65;
			for(int i = controlStart;i < controlEnd;i++)
			{
				//button assigned to this action
				controlPos == i ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
				if(controlPos == i && chooseKeyState == true)
					mRender->DebugPrint(160,starty + (i * 30) - (controlStart * 30)+4,"...");
				else
					mRender->DebugPrint(160,starty + (i * 30) - (controlStart * 30)+4,InputHelper::Instance()->getButtonName(InputHelper::Instance()->getConnection(i).button).c_str());
			}
			mRender->SetDefaultFont();

            if(mRender->GetFontLanguage() == ENGLISH)
            {
                controlPos == InputHelper::ACTION_COUNT ? DrawText(240,269,GU_COLOR(1,1,0,1) ,default_size,"Cancel") : DrawText(240,269,GU_COLOR(1,1,1,1) ,default_size,"Cancel");
                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Controls");
            }
            if(mRender->GetFontLanguage() == RUSSIAN)
            {
                controlPos == InputHelper::ACTION_COUNT ? DrawText(240,269,GU_COLOR(1,1,0,1) ,default_size,"Otmena") : DrawText(240,269,GU_COLOR(1,1,1,1) ,default_size,"Otmena");
                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Naznayenie knopok");
            }
		}
		break;
		case 2://analog stick
		{
			sceGuDisable(GU_DEPTH_TEST);
			sceGuEnable(GU_BLEND);
			sceGuColor(GU_COLOR(1,1,1,1.0f));

            for(int x = 0; x < 8; x++)
            {
                for(int y = 0; y < 5; y++)
                {
                    backSprite->SetPosition(x*64,y*64);
                    backSprite->Draw();
                }
            }

			//something
			nbuttonSprite->SetPosition(240,100);
			nbuttonSprite->Draw();

			nbuttonSprite->SetPosition(240,130);
			nbuttonSprite->Draw();

			nbuttonSprite->SetPosition(240,160);
			nbuttonSprite->Draw();

			nbuttonSprite->SetPosition(240,190);
			nbuttonSprite->Draw();

			//calibrate stick
			buttonSprite->SetPosition(240,230);
			buttonSprite->Draw();

			//back
			buttonSprite->SetPosition(240,260);
			buttonSprite->Draw();

			if(currentAnalogPos < 4)
			{
				sbuttonSprite->SetPosition(240,100 + (currentAnalogPos * 30));
				sbuttonSprite->Draw();
			}else if(currentAnalogPos == 4)
			{
				sbuttonSprite->SetPosition(240,230);
				sbuttonSprite->Draw();
			}else
			{
				sbuttonSprite->SetPosition(240,260);
				sbuttonSprite->Draw();
			}

			moverSprite->SetPosition(144 +((fabs(InputHelper::Instance()->analogYup) * 10) * 20),100);
			moverSprite->Draw();

			moverSprite->SetPosition(144 +((fabs(InputHelper::Instance()->analogYdown) * 10) * 20),130);
			moverSprite->Draw();

			moverSprite->SetPosition(144 +((fabs(InputHelper::Instance()->analogXleft) * 10) * 20),160);
			moverSprite->Draw();

			moverSprite->SetPosition(144 +((fabs(InputHelper::Instance()->analogXright) * 10) * 20),190);
			moverSprite->Draw();

			sceGuDisable(GU_BLEND);
			sceGuEnable(GU_DEPTH_TEST);

            mRender->SetFontStyle(default_size,0xFFFFFFFF,0,0x00000200);
            if(mRender->GetFontLanguage() == ENGLISH)
            {
                currentAnalogPos == 0 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,109,"Analog up : %d%%",(int)(fabs(InputHelper::Instance()->analogYup) * 112.0f));

                currentAnalogPos == 1 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,139,"Analog down : %d%%",(int)(fabs(InputHelper::Instance()->analogYdown) * 112.0f));

                currentAnalogPos == 2 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,169,"Analog left : %d%%",(int)(fabs(InputHelper::Instance()->analogXleft) * 112.0f));

                currentAnalogPos == 3 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,199,"Analog right : %d%%",(int)(fabs(InputHelper::Instance()->analogXright) * 112.0f));

                currentAnalogPos == 4 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,239,"Calibrate stick");

                currentAnalogPos >= 5 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,269,"Cancel");

                mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,49,mSystemMgr->IsAnalogCalibrated() ? "Stick: calibrated" : "Stick: default");

                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Analog stick");
            }
            if(mRender->GetFontLanguage() == RUSSIAN)
            {
                currentAnalogPos == 0 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,109,"Stik vverh : %d%%",(int)(fabs(InputHelper::Instance()->analogYup) * 112.0f));

                currentAnalogPos == 1 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,139,"Stik vniz : %d%%",(int)(fabs(InputHelper::Instance()->analogYdown) * 112.0f));

                currentAnalogPos == 2 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,169,"Stik vlevo : %d%%",(int)(fabs(InputHelper::Instance()->analogXleft) * 112.0f));

                currentAnalogPos == 3 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,199,"Stik vpravo : %d%%",(int)(fabs(InputHelper::Instance()->analogXright) * 112.0f));

                currentAnalogPos == 4 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,239,"Kalibrovka stika");

                currentAnalogPos >= 5 ? mRender->SetFontStyle(default_size,GU_COLOR(1,1,0.25f,1),0,0x00000200) : mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,269,"Otmena");

                mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
                mRender->DebugPrint(240,49,mSystemMgr->IsAnalogCalibrated() ? "Stik: kalibrovan" : "Stik: po umolchaniyu");

                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Nastro~ki stika");
            }
		}
		break;
		case 3://analog stick calibration
		{
			sceGuDisable(GU_DEPTH_TEST);
			sceGuEnable(GU_BLEND);
			sceGuColor(GU_COLOR(1,1,1,1.0f));

            for(int x = 0; x < 8; x++)
            {
                for(int y = 0; y < 5; y++)
                {
                    backSprite->SetPosition(x*64,y*64);
                    backSprite->Draw();
                }
            }

			sceGuDisable(GU_BLEND);
			sceGuEnable(GU_DEPTH_TEST);

            // The live reading this screen is working from, and drawn from the *raw* stick on purpose:
            // this is the number the baseline and the range are about to be derived from, so it has to
            // be the one the player can watch move. A calibrated readout would sit at zero while the
            // stick was being pushed as far as it goes, which is the one thing it must not do here.
            mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);
            mRender->DebugPrint(240,210,"X %d%%   Y %d%%",(int)(mSystemMgr->GetAnalogRawX() * 100.0f),(int)(mSystemMgr->GetAnalogRawY() * 100.0f));

            mRender->SetFontStyle(default_size,GU_COLOR(1,1,1,1),0,0x00000200);

            if(mRender->GetFontLanguage() == ENGLISH)
            {
                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Calibrate stick");

                if(calState == kCalRestIdle)
                {
                    if(calFailed)
                        mRender->DebugPrint(240,110,"The stick was not resting,");
                    mRender->DebugPrint(240,140,"Let go of the stick,");
                    mRender->DebugPrint(240,160,"then press X");
                }
                else if(calState == kCalRestSample)
                {
                    mRender->DebugPrint(240,140,"Hold still...");
                }
                else if(calState == kCalRoll)
                {
                    mRender->DebugPrint(240,120,"Roll the stick all the way");
                    mRender->DebugPrint(240,140,"out to its edges,");
                    mRender->DebugPrint(240,160,"then press X");
                }
                else
                {
                    mRender->DebugPrint(240,140,"Calibrated.");
                    mRender->DebugPrint(240,160,"Press X to save");
                }

                mRender->DebugPrint(240,250,"Circle to cancel");
            }
            if(mRender->GetFontLanguage() == RUSSIAN)
            {
                DrawText(240,29,GU_COLOR(1,1,1,1) ,default_size,"Kalibrovka stika");

                if(calState == kCalRestIdle)
                {
                    if(calFailed)
                        mRender->DebugPrint(240,110,"Stik ne v pokoe,");
                    mRender->DebugPrint(240,140,"Otpusti stik,");
                    mRender->DebugPrint(240,160,"zatem nazhmi X");
                }
                else if(calState == kCalRestSample)
                {
                    mRender->DebugPrint(240,140,"Derzhi nepodvizhno...");
                }
                else if(calState == kCalRoll)
                {
                    mRender->DebugPrint(240,120,"Provedi stik do upora");
                    mRender->DebugPrint(240,140,"po vsem krayam,");
                    mRender->DebugPrint(240,160,"zatem nazhmi X");
                }
                else
                {
                    mRender->DebugPrint(240,140,"Kalibrovka gotova.");
                    mRender->DebugPrint(240,160,"Nazhmi X chtoby sohranit");
                }

                mRender->DebugPrint(240,250,"Krug dlya otmeny");
            }
		}
		break;
	}

	//end frame
	mRender->EndFrame();
}

void StateOptions::DrawText(int x,int y, unsigned int color, float size, const char *message, ...)
{
    mRender->SetFontStyle(size,color,0,0x00000200|0x00000000);
    mRender->DebugPrint(x,y,message);
}
