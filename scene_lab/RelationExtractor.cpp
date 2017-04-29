#include "RelationExtractor.h"
#include "../common/geometry/CModel.h"
#include "../common/geometry/Scene.h"
#include "../t2scene/SceneSemGraph.h"
#include "../common/geometry/SuppPlane.h"

RelationExtractor::RelationExtractor()
{
}


RelationExtractor::~RelationExtractor()
{
}

QString RelationExtractor::getRelationConditionType(CModel *anchorModel, CModel *actModel)
{
	int anchorModelId = anchorModel->getID();
	int actModelId = actModel->getID();

	// test for support relation
	if (actModel->suppParentID == anchorModelId)
	{
		return ConditionName[ConditionType::Pc];
	}

	if (anchorModel->suppParentID == actModelId)
	{
		return ConditionName[ConditionType::Cp];
	}
	
	// test for sibling relation
	if (actModel->suppParentID == anchorModel->suppParentID)
	{
		return ConditionName[ConditionType::Sib];
	}

	QString anchorCatName = anchorModel->getCatName();
	QString actCatName = actModel->getCatName();

	// test for proximity relation
	// do not consider proximity for "room"
	if (anchorCatName != "room" && actCatName != "room" && isInProximity(anchorModel, actModel))
	{
		return ConditionName[ConditionType::Pro];
	}

	return QString("none");
}

std::vector<QString> RelationExtractor::extractSpatialSideRelForModelPair(int anchorModelId, int actModelId)
{
	CModel *anchorModel = m_currScene->getModel(anchorModelId);
	CModel *actModel = m_currScene->getModel(actModelId);

	return extractSpatialSideRelForModelPair(anchorModel, actModel);
}

std::vector<QString> RelationExtractor::extractSpatialSideRelForModelPair(CModel *anchorModel, CModel *actModel)
{
	double angleThreshold = 30;
	double distThreshold = 0.1 / m_currScene->getSceneMetric();

	std::vector<QString> relationStrings;

	QString conditionType = getRelationConditionType(anchorModel, actModel);

	if (conditionType == "none") return relationStrings;

	// add near to proximity objs
	if (conditionType == ConditionName[ConditionType::Pro])
	{
		relationStrings.push_back(PairRelStrings[PairRelation::Near]);  // near
	}

	double sideSectionVal = MathLib::Cos(angleThreshold);
	MathLib::Vector3 refFront = anchorModel->getHorizonFrontDir();
	MathLib::Vector3 refUp = anchorModel->getVertUpDir();

	MathLib::Vector3 refPos = anchorModel->m_currOBBPos;   // OBB bottom center
	MathLib::Vector3 testPos = actModel->m_currOBBPos;

	MathLib::Vector3 fromRefToTestVec = testPos - refPos;
	fromRefToTestVec[2] = 0; // project to XY plane
	fromRefToTestVec.normalize();

	// add front and back to sibling or near objs
	if (conditionType == ConditionName[ConditionType::Sib] || conditionType == ConditionName[ConditionType::Pro])
	{
		// front or back side is not view dependent
		double frontDirDot = fromRefToTestVec.dot(refFront);
		if (frontDirDot > sideSectionVal)
		{
			QString refName = m_currScene->getModelCatName(anchorModel->getID());
			relationStrings.push_back(PairRelStrings[PairRelation::Front]); // front
		}
		else if (frontDirDot < -sideSectionVal && frontDirDot > -1)
		{
			relationStrings.push_back(PairRelStrings[PairRelation::Back]); // back
		}
	}

	// TODO: add OnCenter
	bool isOnCenter = false;
	double toBottomHight = testPos.z - refPos.z;
	double xyPlaneDist = std::sqrt(std::pow(testPos.x-refPos.x, 2) + std::pow(testPos.y-refPos.y, 2));

	if (xyPlaneDist < distThreshold && toBottomHight > 5*distThreshold)
	{
		isOnCenter = true;
	}

	MathLib::Vector3 refRight = refFront.cross(refUp);

	// adjust Right for certain models
	QString refName = m_currScene->getModelCatName(anchorModel->getID());
	if (refName == "desk" || refName == "bookcase" || refName.contains("cabinet") || refName == "dresser"
		|| refName == "monitor"|| refName =="tv")
	{
		refRight = -refRight;
	}

	double rightDirDot = fromRefToTestVec.dot(refRight);

	if (rightDirDot >= sideSectionVal) // right of obj
	{
		if (conditionType == ConditionName[ConditionType::Pc] && !isOnCenter)
		{
			relationStrings.push_back(PairRelStrings[PairRelation::OnRight]); // right
		}
		else if(conditionType == ConditionName[ConditionType::Sib] || conditionType == ConditionName[ConditionType::Pro])
		{
			relationStrings.push_back(PairRelStrings[PairRelation::RightSide]); // right
		}
	}
	else if (rightDirDot < -sideSectionVal)  // left of obj
	{
		if (conditionType == ConditionName[ConditionType::Pc] && !isOnCenter)
		{
			relationStrings.push_back(PairRelStrings[PairRelation::OnLeft]); // right
		}
		else if(conditionType == ConditionName[ConditionType::Sib] || conditionType == ConditionName[ConditionType::Pro])
		{
			relationStrings.push_back(PairRelStrings[PairRelation::LeftSide]); // left
		}
	}

	MathLib::Vector3 refOBBTopCenter = anchorModel->getModelTopCenter();
	MathLib::Vector3 fromRefTopToTestTop = actModel->getModelTopCenter() - refOBBTopCenter;
	fromRefTopToTestTop.normalize();

	if (fromRefTopToTestTop.dot(refUp) < 0)
	{
		if (anchorModel->hasSuppPlane())
		{
			SuppPlane* p = anchorModel->getSuppPlane(0);
			if (p->isCoverPos(testPos.x, testPos.y) && conditionType != ConditionName[ConditionType::Pc])
			{
				relationStrings.push_back(PairRelStrings[PairRelation::Under]); // under
			}
		}
	}

	return relationStrings;
}

bool RelationExtractor::isInProximity(CModel *anchorModel, CModel *actModel)
{
	double sceneMetric = m_currScene->getSceneMetric();
	MathLib::Vector3 sceneUpDir = m_currScene->getUprightVec();

	COBB refOBB = anchorModel->getOBB();
	COBB testOBB = actModel->getOBB();

	double closestBBDist = refOBB.ClosestDist_Approx(testOBB);

	if (closestBBDist < 0.5 / sceneMetric)
	{
		return true;
	}
	else
		return false;
	
}

void RelationExtractor::extractRelativePosForModelPair(CModel *anchorModel, CModel *actModel, RelativePos *relPos)
{
	relPos->m_anchorObjName = anchorModel->getCatName();
	relPos->m_actObjName = actModel->getCatName();

	relPos->m_actObjId = actModel->getID();
	relPos->m_anchorObjId = anchorModel->getID();

	relPos->m_instanceNameHash = QString("%1_%2_%3").arg(relPos->m_anchorObjName).arg(relPos->m_actObjName).arg(relPos->m_conditionName);
	relPos->m_instanceIdHash = QString("%1_%2_%3").arg(relPos->m_sceneName).arg(relPos->m_anchorObjId).arg(relPos->m_actObjId);

	// first transform actModel into the scene and then bring it back using anchor model's alignMat
	relPos->anchorAlignMat = anchorModel->m_WorldBBToUnitBoxMat;

	if (relPos->anchorAlignMat.M[0] >1e10)
	{
		relPos->isValid = false;
		return;
	}

	relPos->actAlignMat = relPos->anchorAlignMat*actModel->getInitTransMat();

	MathLib::Vector3 actModelInitPos = actModel->getOBBInitPos(); // init pos when load the file
	relPos->pos = relPos->actAlignMat.transform(actModelInitPos);

	MathLib::Vector3 anchorModelFrontDir = anchorModel->getFrontDir();
	MathLib::Vector3 actModelFrontDir = actModel->getFrontDir();
	relPos->theta = GetRotAngleR(anchorModelFrontDir, actModelFrontDir, m_currScene->getUprightVec())/MathLib::ML_PI;

	relPos->isValid = true;
}

