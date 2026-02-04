#pragma once

#if IPLUG_EDITOR
#include "IControls.h"
#endif

#include "BassMatrix.h"
#include "open303/Source/DSPCode/rosic_Open303.h"

// A button control that can take a message from the DSP
class SeqLedBtnControl : public IBSwitchControl
{
public:
  SeqLedBtnControl(float x, float y, const IBitmap &bitmap, int paramIdx, rosic::Open303 &in303);
  void OnMsgFromDelegate(int msgTag, int dataSize, const void *pData) override;
  void OnMouseDown(float x, float y, const IMouseMod &mod) override;

private:
  // the embedded core dsp object:
  rosic::Open303 &open303Core;
};

// A button control that can take a message from the DSP
class SeqNoteBtnControl : public IBSwitchControl
{
public:
  SeqNoteBtnControl(float x, float y, const IBitmap &bitmap, int paramIdx);
  static void SetSequencerButtons(std::array<bool, kNumberOfSeqButtons> sequencer, IGraphics *ui);
  void OnMsgFromDelegate(int msgTag, int dataSize, const void *pData) override;
  void OnMouseDown(float x, float y, const IMouseMod &mod) override;

protected:
  int mParamIdx;
};

// The buttons to modify pattern.
class PtnModBtnControl : public IBSwitchControl
{
public:
  PtnModBtnControl(float x, float y, const IBitmap &bitmap, int paramIdx);
  void OnMouseDown(float x, float y, const IMouseMod &mod) override;
  void OnMouseUp(float x, float y, const IMouseMod &mod) override;
};

////////////////////////////////////////////////////////////////////////////////////////
// GroupBtnControl
// A button that turns off all the other buttons in the group when selected
////////////////////////////////////////////////////////////////////////////////////////
class GroupBtnControl : public IBSwitchControl
{
public:
  GroupBtnControl(float x,
                  float y,
                  const IBitmap &bitmap,
                  int paramIdx,
                  int &selectedGroupTag,
                  int thisTag,
                  int groupStart,
                  int nrOfGroupMembers);
  void OnMsgFromDelegate(int msgTag, int dataSize, const void *pData) override;
  void OnMouseDown(float x, float y, const IMouseMod &mod) override;

protected:
  int &mSelectedGroupTag;
  int mThisTag;
  int mGroupStart;
  int mNrOfGroupMembers;
};
