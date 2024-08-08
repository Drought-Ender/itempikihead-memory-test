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
#include "Game/NaviState.h"
#include "Game/MapMgr.h"

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
		Item* result = &mMonoObjectMgr.mArray[PikiMgr::sReplaceIndex];
		mMonoObjectMgr.mOpenIds[PikiMgr::sReplaceIndex] = false;
		mMonoObjectMgr.mActiveCount++;
		return result;

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

bool Item::interactFue(InteractFue& whistle)
{
	if (canPullout() != false && isAlive()) {
		Navi* navi = static_cast<Navi*>(whistle.mCreature);
		if (!navi->getOlimarData()->hasItem(OlimarData::ODII_ProfessionalNoisemaker)) {
			return false;
		}

		if (gameSystem->isVersusMode()) {
			if (mColor == navi->mNaviIndex) {
				return false;
			}
		}

		PikiMgr::mBirthMode    = PikiMgr::PSM_Force;
		PikiMgr::sReplaceIndex = _184; 
		Piki* piki          = pikiMgr->birth();
		PikiMgr::mBirthMode = PikiMgr::PSM_Normal;

		if (piki) {
			P2ASSERTLINE(701, whistle.mCreature->isNavi());
			kill(nullptr);
			setAlive(false);

			piki->init(nullptr);
			piki->changeShape(mColor);
			piki->changeHappa(mHeadType);
			piki->mNavi = navi;
			piki->setPosition(mPosition, false);
			piki->mFsm->transit(piki, PIKISTATE_AutoNuki, nullptr);
			return true;
		}
	}
	return false;
}

void Item::doAI()
{
	mFsm->exec(this);
	if (mAutopluckedTimer > 0.0f) {
		mAutopluckedTimer -= sys->mDeltaTime;
		if (mAutopluckedTimer <= 0.0f) {
			PikiMgr::mBirthMode    = PikiMgr::PSM_Force;
			PikiMgr::sReplaceIndex = _184;
			Piki* piki          = pikiMgr->birth();
			PikiMgr::mBirthMode = PikiMgr::PSM_Normal;
			if (piki) {
				kill(nullptr);
				setAlive(false);

				piki->init(nullptr);
				piki->changeShape(mColor);
				piki->changeHappa(mHeadType);
				piki->mNavi = nullptr;
				piki->setPosition(mPosition, false);
				piki->mFsm->transit(piki, PIKISTATE_AutoNuki, nullptr);
			} else {
				JUT_PANICLINE(603, "exit failed !!\n");
			}
		}
	}
}

} // namespace ItemPikiHead


// birthPiki__Q24Game19NaviNukuAdjustStateFPQ24Game4Navi
void NaviNukuAdjustState::birthPiki(Navi* navi) {
	OSReport("void NaviNukuAdjustState::birthPiki(Navi* navi)\n");
	PikiMgr::mBirthMode = PikiMgr::PSM_Force;
	PikiMgr::sReplaceIndex = mPikiHead->_184;
	Piki* piki          = pikiMgr->birth();
	PikiMgr::mBirthMode = PikiMgr::PSM_Normal;

	if (!piki) {
		if (mIsFollowing) {
			transit(navi, NSID_Follow, nullptr);
		} else {
			transit(navi, NSID_Walk, nullptr);
		}
		return;
	}

	u16 color = mPikiHead->mColor;
	u16 happa = mPikiHead->mHeadType;
	Vector3f sproutPos = mPikiHead->getPosition();

	mPikiHead->kill(nullptr);
	mPikiHead = nullptr;

	piki->init(nullptr);
	piki->changeShape(color);
	piki->changeHappa(happa);

	
	piki->setPosition(sproutPos, false);
	

	NukareStateArg nukareArg;
	nukareArg.mIsPlucking = navi->mPluckingCounter != 0;
	nukareArg.mNavi       = navi;
	piki->mFsm->transit(piki, PIKISTATE_Nukare, &nukareArg);

	NaviNukuArg nukuArg;
	nukuArg.mIsFollowing = mIsFollowing;

	transit(navi, NSID_Nuku, &nukuArg);
}

bool PikiFallMeckState::becomePikihead(Piki* piki)
{
	OSReport("bool PikiFallMeckState::becomePikihead(Piki* piki)\n");
	bool check;
	if (GameStat::mePikis >= 99) {
		return false;
	} else {
		PikiMgr::mBirthMode        = PikiMgr::PSM_Force;
		PikiMgr::sReplaceIndex     = piki->mMgrIndex;
		ItemPikihead::Item* sprout = static_cast<ItemPikihead::Item*>(ItemPikihead::mgr->birth());
		PikiMgr::mBirthMode        = PikiMgr::PSM_Normal;

		Vector3f pikiPos = piki->getPosition();
		pikiPos.y        = mapMgr->getMinY(pikiPos);
		if (sprout) {
			if (piki->inWater()) {
				efx::TEnemyDive fxDive;
				efx::ArgScale fxArg(pikiPos, 1.2f);

				fxDive.create(&fxArg);
			} else {
				efx::createSimplePkAp(pikiPos);
				piki->startSound(PSSE_PK_SE_ONY_SEED_GROUND, true);
			}

			ItemPikihead::InitArg initArg((EPikiKind)piki->mPikiKind, Vector3f::zero, 1, 0, -1.0f);

			if (mDoAutoPluck) {
				initArg.mAutopluckTimer = 10.0f + 3.0f * sys->mDeltaTime;
			}
			CreatureKillArg killArg(CKILL_DontCountAsDeath);
			piki->kill(&killArg);

			sprout->init(&initArg);
			sprout->setPosition(pikiPos, false);

			

			return true;
		}
	}
	return false;
}

void PikiFallMeckState::bounceCallback(Piki* piki, Sys::Triangle* triangle)
{
	bool check;
	if (mDoAutoPluck && triangle && ItemPikihead::mgr) {
		if (becomePikihead(piki)) {
			return;
		}
	} else if (triangle && !triangle->mCode.isBald() && piki->might_bury() && ItemPikihead::mgr) {
		if (becomePikihead(piki)) {
			return;
		}
	}

	transit(piki, PIKISTATE_Walk, nullptr);
}

} // namespace Game
