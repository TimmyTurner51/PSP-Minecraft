#include <Aurora/System/SystemManager.h>
#include <math.h>

namespace Aurora
{
	namespace System
	{
		SystemManager SystemManager::m_SystemManager;

		SystemManager* SystemManager::Instance()
		{
			return &m_SystemManager;
		}

		void SystemManager::ConfigureDialog(pspUtilityMsgDialogParams *dialog, size_t dialog_size)
		{
		    memset(dialog, 0, dialog_size);

		    dialog->base.size = dialog_size;
		    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE,&dialog->base.language); // Prompt language
		    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN,&dialog->base.buttonSwap); // X/O button swap

		    dialog->base.graphicsThread = 0x11;
		    dialog->base.accessThread = 0x13;
		    dialog->base.fontThread = 0x12;
		    dialog->base.soundThread = 0x10;
		}

		void SystemManager::ShowMessage(const char *message)
		{
			ConfigureDialog(&dialog, sizeof(dialog));
			dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
			dialog.options = PSP_UTILITY_MSGDIALOG_OPTION_TEXT;

			strcpy(dialog.message, message);

			sceUtilityMsgDialogInitStart(&dialog);

			for(;;)
			{

				Graphics::RenderManager::InstancePtr()->StartDialog();

				switch(sceUtilityMsgDialogGetStatus()) {

				case 2:
					sceUtilityMsgDialogUpdate(1);
					break;

				case 3:
					sceUtilityMsgDialogShutdownStart();
					break;

				case 0:
					return;

				}

				Graphics::RenderManager::InstancePtr()->EndDialog();
			}
		}

		bool SystemManager::ShowNetworkDialog()
		{
			bool done = true;
			int result = -1;

			memset(&networkData, 0, sizeof(pspUtilityNetconfData));
			networkData.base.size = sizeof(networkData);
			sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &networkData.base.language);
			sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &networkData.base.buttonSwap);
			networkData.base.graphicsThread = 17;
			networkData.base.accessThread = 19;
			networkData.base.fontThread = 18;
			networkData.base.soundThread = 16;
			networkData.action = PSP_NETCONF_ACTION_CONNECTAP;

			struct pspUtilityNetconfAdhoc adhocparam;
			memset(&adhocparam, 0, sizeof(adhocparam));
			networkData.adhocparam = &adhocparam;

			sceUtilityNetconfInitStart(&networkData);

			while(done)
			{
				Graphics::RenderManager::InstancePtr()->StartDialog();

				switch(sceUtilityNetconfGetStatus())
				{
					case PSP_UTILITY_DIALOG_NONE:
					{
						result = networkData.base.result;
						done = false;
					}
					break;

					case PSP_UTILITY_DIALOG_VISIBLE:
						sceUtilityNetconfUpdate(1);
						break;

					case PSP_UTILITY_DIALOG_QUIT:
						sceUtilityNetconfShutdownStart();
					break;

					case PSP_UTILITY_DIALOG_FINISHED:
					break;

					default:
					break;
				}

				Graphics::RenderManager::InstancePtr()->EndDialog();
			}

			if(result == 0)
				return true;

			return false;
		}

		int SystemManager::ShowMessageYesNo(const char *message)
		{
			ConfigureDialog(&dialog, sizeof(dialog));
			dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_TEXT;
			dialog.options = PSP_UTILITY_MSGDIALOG_OPTION_TEXT;
			dialog.options |= PSP_UTILITY_MSGDIALOG_OPTION_YESNO_BUTTONS|PSP_UTILITY_MSGDIALOG_OPTION_DEFAULT_NO;

			strcpy(dialog.message, message);

			sceUtilityMsgDialogInitStart(&dialog);

			bool done = true;

			while(done)
			{
				Graphics::RenderManager::InstancePtr()->StartDialog();

				switch(sceUtilityMsgDialogGetStatus()) {

				case 2:
					sceUtilityMsgDialogUpdate(1);
					break;

				case 3:
					sceUtilityMsgDialogShutdownStart();
					break;

				case 0:
					done = false;
					break;

				}

				Graphics::RenderManager::InstancePtr()->EndDialog();
			}

			//the return value
			if(dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_YES)
				return 1;
			else if(dialog.buttonPressed == PSP_UTILITY_MSGDIALOG_RESULT_NO)
				return -1;
			else
				return 0;
		}

		int SystemManager::ShowOSK(unsigned short *descritpion,unsigned short *outtext,int maxtextinput)
		{
			//osk params
			SceUtilityOskData oskData;
			SceUtilityOskParams oskParams;
			//init osk data
			memset(&oskData, 0, sizeof(SceUtilityOskData));
			oskData.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT; // Use system default for text input
			oskData.lines = 1;
			oskData.unk_24 = 1;
			oskData.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL; // Allow all input types
			oskData.desc = descritpion;
			//oskData.intext = intext[i];
			int limit = maxtextinput;
			if(limit < 1)
				limit = 1;
			if(limit > 128)
				limit = 128;
			oskData.outtextlength = limit;
			oskData.outtextlimit = limit;
			oskData.outtext = outtext;

			//init osk dialog
			memset(&oskParams, 0, sizeof(SceUtilityOskParams));
			oskParams.base.size = sizeof(oskParams);
			sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &oskParams.base.language);
			sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &oskParams.base.buttonSwap);
			oskParams.base.graphicsThread = 17;
			oskParams.base.accessThread = 19;
			oskParams.base.fontThread = 18;
			oskParams.base.soundThread = 16;
			oskParams.datacount = 1;
			oskParams.data = &oskData;

			sceUtilityOskInitStart(&oskParams);

			bool done = true;

			while(done)
			{
				Graphics::RenderManager::InstancePtr()->StartDialog();

				switch(sceUtilityOskGetStatus())
				{
					case PSP_UTILITY_DIALOG_INIT:
						break;

					case PSP_UTILITY_DIALOG_VISIBLE:
						sceUtilityOskUpdate(1);
						break;

					case PSP_UTILITY_DIALOG_QUIT:
						sceUtilityOskShutdownStart();
						break;

					case PSP_UTILITY_DIALOG_FINISHED:
						break;

					case PSP_UTILITY_DIALOG_NONE:
						done = false;

					default :
						break;
				}

				Graphics::RenderManager::InstancePtr()->EndDialog();
			}

			if(oskData.result == PSP_UTILITY_OSK_RESULT_CANCELLED)
				return -1;

			return 0;
		}

		void SystemManager::ShowMessageError(const char *message,int error)
		{
			ConfigureDialog(&dialog, sizeof(dialog));
			dialog.mode = PSP_UTILITY_MSGDIALOG_MODE_ERROR;
			dialog.options = PSP_UTILITY_MSGDIALOG_OPTION_ERROR;
			dialog.errorValue = error;
			strcpy(dialog.message, message);

			sceUtilityMsgDialogInitStart(&dialog);

			bool done = true;

			while(done)
			{

				Graphics::RenderManager::InstancePtr()->StartDialog();

				int test = sceUtilityMsgDialogGetStatus();

				switch(test)
				{

				case 2:
				sceUtilityMsgDialogUpdate(1);
				break;

				case 3:
				sceUtilityMsgDialogShutdownStart();
				break;

				case 0:
				done = false;
				break;

				}

				Graphics::RenderManager::InstancePtr()->EndDialog();
			}
		}

		void SystemManager::InputUpdate()
		{
			//new to old
			oldPadData = newPadData;
			//get input data
			sceCtrlReadBufferPositive(&newPadData, 1);
			//sceCtrlPeekBufferPositive(&newPadData, 1);
		}

		bool SystemManager::KeyPressed(PspCtrlButtons button)
		{
			if (newPadData.Buttons != oldPadData.Buttons)
			{
				if (newPadData.Buttons & button)
				{
					return true;
				}
			}
			return false;
		}

		bool SystemManager::KeyHold(PspCtrlButtons button)
		{
			if (newPadData.Buttons & button)
			{
				return true;
			}

			return false;
		}

		float SystemManager::GetAnalogX()
		{
			float x = GetAnalogRawX();
			if(analogCalibrated == false)
			{
				return x;
			}
			x = (x - analogBaseX) / analogRangeX;
			if(x > 1.0f)
			{
				x = 1.0f;
			}
			if(x < -1.0f)
			{
				x = -1.0f;
			}
			return x;
		}

		float SystemManager::GetAnalogY()
		{
			float y = GetAnalogRawY();
			if(analogCalibrated == false)
			{
				return y;
			}
			y = (y - analogBaseY) / analogRangeY;
			if(y > 1.0f)
			{
				y = 1.0f;
			}
			if(y < -1.0f)
			{
				y = -1.0f;
			}
			return y;
		}

		// The pad's nominal reading and half-range. The 122.5 is the constant this build has always used,
		// and it is deliberately left alone: a healthy stick rests a little above 122.5, so this reads
		// about +0.04 at rest, but the same number feeds every threshold in the game. Calibration is the
		// place to correct that, because it corrects it per-console and it can be undone from Settings
		// -- whereas changing the constant here would silently re-tune every menu in the game at once.
		float SystemManager::GetAnalogRawX()
		{
			return (((float)newPadData.Lx - 122.5f)/122.5f);
		}

		float SystemManager::GetAnalogRawY()
		{
			return (((float)newPadData.Ly - 122.5f)/122.5f);
		}

		void SystemManager::SetAnalogCalibration(float baseX, float baseY, float rangeX, float rangeY)
		{
			// A range of zero would divide the axis to infinity, and a small one would amplify the noise
			// instead of the travel. Both are what an aborted calibration collects, so they are rejected
			// here rather than trusted from the caller.
			if(rangeX < 0.25f || rangeY < 0.25f)
			{
				return;
			}
			analogBaseX = baseX;
			analogBaseY = baseY;
			analogRangeX = rangeX;
			analogRangeY = rangeY;
			analogCalibrated = true;
		}

		void SystemManager::ClearAnalogCalibration()
		{
			analogCalibrated = false;
			analogBaseX = 0.0f;
			analogBaseY = 0.0f;
			analogRangeX = 1.0f;
			analogRangeY = 1.0f;
		}

		bool SystemManager::IsAnalogCalibrated() const
		{
			return analogCalibrated;
		}

		void SystemManager::GetAnalogCalibration(float& baseX, float& baseY, float& rangeX, float& rangeY) const
		{
			baseX = analogBaseX;
			baseY = analogBaseY;
			rangeX = analogRangeX;
			rangeY = analogRangeY;
		}

		void SystemManager::GetAnalogDeadzone(float dead, float& x, float& y)
		{
			x = GetAnalogX();
			y = GetAnalogY();
			float mag = sqrtf(x * x + y * y);
			if(mag <= dead)
			{
				x = 0.0f;
				y = 0.0f;
				return;
			}
			float t = (mag - dead) / (1.0f - dead);
			if(t > 1.0f)
			{
				t = 1.0f;
			}
			x = x / mag * t;
			y = y / mag * t;
		}

		u32 SystemManager::ramAvailableLineareMax (void)
		{
			return sceKernelMaxFreeMemSize();
		}

		void SystemManager::Init()
		{

		}

		u32 SystemManager::ramAvailable (void)
		{
			return sceKernelTotalFreeMemSize();
		}
	}
}
