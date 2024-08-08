#include "Dolphin/rand.h"
#include "Game/AIConstants.h"
#include "Game/Entities/ItemPikihead.h"
#include "Game/gameStat.h"
#include "Game/Piki.h"
#include "Game/PikiMgr.h"
#include "Game/PikiState.h"
#include "Game/Navi.h"
#include "Game/MoviePlayer.h"
#include "efx/TEnemyDive.h"
#include "nans.h"
#include "Radar.h"
#include "SoundID.h"

namespace Game
{
namespace ItemPikihead
{


/**
 * @note Address: 0x801D9AC4
 * @note Size: 0x21C
 */
void Item::onInit(CreatureInitArg* settings)
{
	mModel             = mgr->createPikiheadModel(this);
	mAnimator.mAnimMgr = mgr->mAnimMgr;
	mAnimator.startAnim(0, nullptr);
	mCollTree->attachModel(mModel);
	setAtari(false);
	InitArg* itemInitArg = static_cast<InitArg*>(settings);
	if (itemInitArg) {
		mColor = itemInitArg->mPikminType;
		if (itemInitArg->mPikminType == -1) {
			mColor = Blue;
		}
		mVelocity         = itemInitArg->mVelocity;
		mHeadType         = itemInitArg->mHeadType;
		mAutopluckedTimer = itemInitArg->mAutopluckTimer;
	} else {
		mColor            = randFloat() * 5.0f;
		mHeadType         = Leaf;
		mAutopluckedTimer = -1.0f;
	}
	mEfxTane->init();
	mEfxTane->mPikiColor   = mColor;
	mEfxTane->mObjPos      = &mPosition;
	mEfxTane->mEfxPos      = &mEfxPosition;
	mEfxTane->mObjMatrix   = &mBaseTrMatrix;
	mEfxTane->mHappaJntMtx = mModel->getJoint("happajnt3")->getWorldMatrix();
	if (itemInitArg && itemInitArg->mIsAlreadyBuried) {
		mFsm->start(this, PIKIHEAD_Hatuga, nullptr);
	} else {
		mFsm->start(this, PIKIHEAD_Fall, nullptr);
	}

	setAlive(true);
	if (itemInitArg && itemInitArg->mPikminType != -1) {
		GameStat::mePikis.inc(mColor);
	}
	Radar::Mgr::entry(this, Radar::MAP_BURIED_PIKMIN, 0);
}

/**
 * @note Address: 0x801DA668
 * @note Size: 0xD4
 */
Mgr::Mgr()
    : FixedSizeItemMgr<Item>()
{
	mItemName = "PikiHead";
	setModelSize(1);
	mObjectPathComponent = "user/Kando/objects/pikihead";

	alloc(100);

	onAlloc();
}

int Mgr::getEmptyIndex() {
	if (!pikiMgr) return mMonoObjectMgr.getEmptyIndex();

	for (int i = 0; i < mMonoObjectMgr.mMax; i++) {
		if (mMonoObjectMgr.mOpenIds[i] && pikiMgr->mOpenIds[i]) {
			return i;
		}
	}
	return -1;
}

Item* Mgr::birth()
{
	switch (PikiMgr::mBirthMode) {
	case PikiMgr::PSM_Normal: // don't make a sprout if we're at or over 100 pikmin on the field
		if (pikiMgr->mActiveCount + mMonoObjectMgr.mActiveCount >= 100) {
			return nullptr;
		}
		break;

	case PikiMgr::PSM_Force: // just make the damn sprout
		break;

	case PikiMgr::PSM_Replace:                      // we should not be entering a cave floor and immediately making a sprout lol
		JUT_PANICLINE(834, "‚±‚ê‚Í‚ ‚è‚¦‚È‚¢‚æ\n"); // 'this is impossible' lol
		break;
	}

	int index = getEmptyIndex();
	if (index != -1) {
		Item* result = &mMonoObjectMgr.mArray[index];
		mMonoObjectMgr.mOpenIds[index] = false;
		mMonoObjectMgr.mActiveCount++;
		return result;
	}
	return nullptr;
}

SysShape::Model* Mgr::createPikiheadModel(Item* item) {

	SysShape::Model* model = pikiMgr->createModel(7, item->_184);
	onCreateModel(model);
	return model;
}

/**
 * @note Address: 0x801DA94C
 * @note Size: 0xAC
 */
void Mgr::onLoadResources()
{

	mModelData[0] = pikiMgr->mPikiModels[7];

	loadArchive("arc.szs");
	// loadBmd("pikihead.bmd", 0, 0x20000);
	// (*mModelData)->newSharedDisplayList(0x40000);
	JKRArchive* arc = openTextArc("texts.szs");
	loadAnimMgr(arc, "pikiheadAnimMgr.txt");
	closeTextArc(arc);
	// createMgr(100, 0x80000);
}


} // namespace ItemPikiHead

} // namespace Game
