#pragma once
#include "../common/utilities/utility.h"

class RelativePos
{
public:
	RelativePos() {};
	~RelativePos() {};

	MathLib::Vector3 pos;  // rel pos of actObj in anchor's unit frame
	double theta;
	MathLib::Matrix4d anchorAlignMat;  // transformation of anchorObj to unit frame
	MathLib::Matrix4d actAlignMat;  // transformation of actObj to anchorObj's unit frame,  anchorAlignMat*actInitTransMat
	double unitFactor;

	QString m_actObjName;
	QString m_anchorObjName;

	QString m_conditionName;

	int m_actObjId;
	int m_anchorObjId;
	QString m_sceneName;

	QString m_instanceHash;

	bool isValid;
};

struct GaussianModel
{
	int dim;
	Eigen::VectorXd mean; 
	Eigen::MatrixXd covarMat;
	
	double weight;   // mixing weight
};

class PairwiseRelationModel
{
public:
	PairwiseRelationModel(const QString &anchorName, const QString &actName, const QString &conditionName, const QString & relationName = "general");
	~PairwiseRelationModel() {};

	void fitGMM(int instanceTh);
	void output(QTextStream &ofs);

	int m_numGauss;
	int m_numInstance;

	std::vector<RelativePos*> m_instances;
	std::vector<GaussianModel> m_gaussians;

	QString m_actObjName;
	QString m_anchorObjName;

	QString m_conditionName; // parent-child, sibling, proximity, or text-specified relation
	QString m_relationName;  // none, left, right, etc.
};

struct OccurrenceModel
{
	std::map<QString, double> m_objOccurProbs;

	int instNum;
};

class GroupRelationModel
{
public: 
	GroupRelationModel();
	~GroupRelationModel();

	void fitGMMs();
	void output(QTextStream &ofs);

public:
	std::map<QString, PairwiseRelationModel*> m_pairwiseModels;  // relation-conditioned relative model
	OccurrenceModel m_occurrenceModel;  // relation-conditioned occurrence model

	QString m_anchorObjName;
	QString m_relationName;
};

