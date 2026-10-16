#ifndef AURORA_SYSTEMMANAGER_H
#define AURORA_SYSTEMMANAGER_H

#include <malloc.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <psputility.h>
#include <string.h>

#include <Aurora/Graphics/RenderManager.h>

#define RAM_BLOCK      (1024 * 1024)

namespace Aurora
{
	namespace System
	{
		class SystemManager
		{
		private:

			//static access
			static SystemManager m_SystemManager;

			//dialog params
			pspUtilityMsgDialogParams dialog;
			pspUtilityNetconfData networkData;
			void ConfigureDialog(pspUtilityMsgDialogParams *dialog, size_t dialog_size);

			//memory
			u32 ramAvailableLineareMax (void);

			//input
			SceCtrlData oldPadData,newPadData;

			// Analog stick calibration, in the normalised units GetAnalogRawX/Y return. `base` is the
			// measured resting offset and `range` is the measured distance from that base out to the
			// rim, per axis. Defaults describe an uncalibrated stick, and `analogCalibrated` gates their
			// use, so an uncalibrated stick reads exactly as it did before any of this existed.
			bool  analogCalibrated = false;
			float analogBaseX = 0.0f;
			float analogBaseY = 0.0f;
			float analogRangeX = 1.0f;
			float analogRangeY = 1.0f;

		public:


			static SystemManager* Instance();

			void Init();

			//memory
			u32 ramAvailable (void);

			//system dialogs
			void ShowMessage(const char *message);
			void ShowMessageError(const char *message,int error);
			int ShowMessageYesNo(const char *message);
			int ShowOSK(unsigned short *descritpion,unsigned short *outtext,int maxtextinput);
			bool ShowNetworkDialog();

			//input
			void InputUpdate();
			bool KeyPressed(PspCtrlButtons button);
			bool KeyHold(PspCtrlButtons button);
			float GetAnalogX();
			float GetAnalogY();
			// The stick as a radial vector with a deadzone applied. Inside `dead` the stick reads exactly
			// centred, which is what stops a worn stick from walking a cursor round on its own; past it
			// the magnitude is rescaled so the far edge of the stick still reaches full speed. Radial
			// rather than per-axis, because a per-axis test leaves the diagonals the most sensitive
			// direction on the stick, which is the opposite of what the hand expects.
			void GetAnalogDeadzone(float dead, float& x, float& y);

			// --- Analog stick calibration ---------------------------------------------------------
			// A worn stick does not rest at the pad's nominal centre, and it does not reach the same
			// distance from that centre on every axis. A deadzone has to be wide enough for the worst
			// stick it will ever meet, so it throws away fine control on a good one; calibration instead
			// measures the stick in front of you. `base` is the resting reading and `range` the distance
			// from it to the rim, so applying the result is a subtraction and a divide -- after which
			// "off centre" really is 0 and the extremes really are +-1, and every existing threshold in
			// the game (the menu repeats, the analog actions, the deadzones) is measuring the stick's
			// actual travel rather than its nominal travel.
			void SetAnalogCalibration(float baseX, float baseY, float rangeX, float rangeY);
			void ClearAnalogCalibration();
			bool IsAnalogCalibrated() const;
			void GetAnalogCalibration(float& baseX, float& baseY, float& rangeX, float& rangeY) const;
			// The stick with no calibration applied. The calibration screen samples *this*, because it
			// cannot be measuring its own correction while it is trying to measure the stick.
			float GetAnalogRawX();
			float GetAnalogRawY();

		};
	}
}

#endif

