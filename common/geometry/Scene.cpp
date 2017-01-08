﻿#include "Scene.h"
#include "RelationGraph.h"
//#include "SuppPlane.h"
//#include "../action/Skeleton.h"
#include "../utilities/utility.h"
//#include "../utilities/rng.h"
#include "../utilities/mathlib.h"
#include "CMesh.h"
//#include "ICP.h"

#include <QFileDialog>
#include <QTextStream>

CScene::CScene()
{
	m_modelNum = 0;
	m_uprightVec = MathLib::Vector3(0, 0, 1.0);
	m_metric = 1.0;
	m_floorHeight = 0;

	m_roomID = -1;

	m_relationGraph = NULL;
	m_hasRelGraph = false;
	m_hasSupportHierarchy = false;

	m_showSceneGaph = false;
	m_showModelOBB = false;
	m_showSuppPlane = false;
	m_showModelFrontDir = false;
	m_showSuppChildOBB = true;
}

CScene::~CScene()
{
	for (int i = 0; i < m_modelNum; i++)
	{
		delete m_modelList[i];
	}

	m_modelList.clear();
	m_modelNameIdMap.clear();

	if (m_relationGraph!=NULL)
	{
		delete m_relationGraph;
	}

	m_showSceneGaph = false;
	m_showModelOBB = false;
	m_showSuppPlane = false;
}

void CScene::loadSceneFromFile(const QString &filename, int metaDataOnly, int obbOnly, int meshOnly)
{

	QFile inFile(filename);
	QTextStream ifs(&inFile);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) return;

	QFileInfo sceneFileInfo(inFile.fileName());
	m_sceneFileName = sceneFileInfo.baseName();   // scene_01.txt
	m_sceneFilePath = sceneFileInfo.absolutePath();  

	int cutPos = sceneFileInfo.absolutePath().lastIndexOf("/");  
	m_sceneDBPath = sceneFileInfo.absolutePath().left(cutPos);  

	if (metaDataOnly)
	{
		std::cout << "\nLoading scene meta data: " << m_sceneFileName.toStdString() << "...\n";
	}
	else
	{
		std::cout << "\nLoading scene: " << m_sceneFileName.toStdString() << "...\n";
	}
	

	QString databaseType;
	QString modelFileName;

	ifs >> databaseType;

	m_sceneFormat = databaseType;

	if (!obbOnly)
	{
		if (meshOnly)
		{
			std::cout << "\tloading objects without obb...\n";
		}
		else
		{
			std::cout << "\tloading objects with obb...\n";
		}
	}
	else
	{
		std::cout << "\tloading objects with obb only...\n";
	}

	if (databaseType == QString("StanfordSceneDatabase") || databaseType == QString("SceneNNConversionOutput"))
	{
		m_metric = 0.0254; // convert from 1 inch to 1 meter

		if (databaseType == QString("SceneNNConversionOutput"))
		{
			m_metric = 1.0;
		}

		int currModelID = -1;

		m_modelDBPath = m_sceneDBPath + "/models";  

		while (!ifs.atEnd())
		{
			QString currLine = ifs.readLine();
			if (currLine.contains("modelCount "))
			{
				m_modelNum = StringToIntegerList(currLine.toStdString(), "modelCount ")[0];
				m_modelList.resize(m_modelNum);
			}

			if (currLine.contains("newModel "))
			{
				std::vector<std::string> parts = PartitionString(currLine.toStdString(), " ");
				int modelIndex = StringToInt(parts[1]);

				CModel *newModel = new CModel();
				QString modelNameString = parts[2].c_str();
				newModel->loadModel(m_modelDBPath + "/" + modelNameString + ".obj", 1.0, metaDataOnly, obbOnly, meshOnly, databaseType);
				newModel->setSceneUpRightVec(m_uprightVec);

				currModelID += 1;
				newModel->setID(currModelID);
				newModel->setSceneMetric(m_metric);

				m_modelList[currModelID] = newModel;

				m_modelCatNameList.push_back(newModel->getCatName());

				if (modelNameString.contains("room"))
				{
					m_roomID = currModelID;
				}
			}

			if (currLine.contains("transform "))
			{
				std::vector<float> transformVec = StringToFloatList(currLine.toStdString(), "transform ");
				//Eigen::Map<Eigen::Matrix4f> trasMat(transformVec.data());

				//Eigen::Matrix4f transMat(transformVec.data()); // transformation vector in scene file is column wise as Eigen

				MathLib::Matrix4d transMat(transformVec);

				//if (databaseType == "SceneNNConversionOutput")
				//{
				//	MathLib::Matrix4d rotMat = GetRotMat(MathLib::Vector3(1, 0, 0), -MathLib::ML_PI_2);
				//	transMat = rotMat*transMat;
				//}
				
				m_modelList[currModelID]->setInitTransMat(transMat);
				m_modelList[currModelID]->transformModel(transMat);

				std::cout << "\t\t" << currModelID + 1 << "/" << m_modelNum << " models loaded\r";
			}
		}
	}

	std::cout << "\n";

	m_relationGraph = new RelationGraph(this);

	QString graphFilename = m_sceneFilePath + "/" + m_sceneFileName + ".sg";

	if (m_relationGraph->readGraph(graphFilename) != -1)
	{
		m_hasRelGraph = true;
		buildSupportHierarchy();
		std::cout << "\tstructure graph loaded\n";
	}

	if (metaDataOnly)
	{
		std::cout << "Scene loaded\n";
		return;
	}

	computeAABB();
	buildModelDislayList();

	std::cout << "Scene loaded\n";
}

void CScene::buildRelationGraph()
{
	std::cout << "\tstart build relation graph for "<< m_sceneFileName.toStdString()<< "...";

	QString graphFilename = m_sceneFilePath + "/" + m_sceneFileName + ".sg";

	if (m_sceneFormat == "SceneNNConversionOutput")
	{
		for (int i = 0; i < m_modelNum; i++)
		{
			m_modelList[i]->computeOBB(2);
		}
	}

	m_relationGraph->buildGraph();
	m_relationGraph->saveGraph(graphFilename);

	buildSupportHierarchy();
	m_hasRelGraph = true;
	std::cout << " done.\n";
}

void CScene::buildModelDislayList(int showDiffColor /*= 1*/, int showFaceCluster /*= 0*/)
{
	foreach(CModel *m, m_modelList)
	{
		m->buildDisplayList(showDiffColor, showFaceCluster);
	}
}

void CScene::draw()
{
	if (m_showSceneGaph && m_hasRelGraph)
	{
		m_relationGraph->drawGraph();

		foreach(CModel *m, m_modelList)
		{
			m->draw(0, 1, m_showSuppPlane, m_showModelFrontDir, m_showSuppChildOBB);  // only show obb
		}
	}
	else
	{
		foreach(CModel *m, m_modelList)
		{
			m->draw(1, m_showModelOBB, m_showSuppPlane, m_showModelFrontDir, m_showSuppChildOBB);
		}
	}
}

void CScene::computeAABB()
{
	// init aabb
	m_AABB = m_modelList[0]->getAABB();

	// update scene bbox
	for (int i = 1; i < m_modelList.size(); i++)
	{
		if (!m_modelList[i]->isVisible())
		{
			continue;
		}

		CAABB curbox = m_modelList[i]->getAABB();
		m_AABB.Merge(curbox);
	}
}

bool CScene::isSegIntersectModel(MathLib::Vector3 &startPt, MathLib::Vector3 &endPt, int modelID, double radius)
{
	return m_modelList[modelID]->isSegIntersectMesh(startPt, endPt, radius);
}

void CScene::prepareForIntersect()
{
		for (int i = 0; i < m_modelNum; i++)
		{
			m_modelList[i]->prepareForIntersect();
		}
}

//void CScene::buildModelSuppPlane()
//{
//	for (int i = 0; i < m_modelNum; i++)
//	{
//		m_modelList[i]->buildSuppPlane();
//
//		QString catName = m_modelList[i]->getCatName();
//		if (catName.contains("room"))
//		{
//			//m_floor = m_modelList[i]->getSuppPlane(0);
//			if (m_modelList[i]->getModelFileName() == "room01")
//			{
//				m_floorHeight = 0.074;
//			}
//
//		}
//	}
//}

double CScene::getFloorHeight()
{
	//return m_floor->GetZ();
	return m_floorHeight;
}

std::vector<int> CScene::getModelIdWithCatName(QString s, bool usingSynset)
{
	std::vector<int> idList;

	for (int i = 0; i < m_modelNum; i++)
	{
		QString synS;
		QString tempS = s;

		// find candidates in the scene
		if (s == "coffeetable")
		{
			synS = "sidetable";
		}

		if (s == "officechair" || s=="diningchair")
		{
			synS = "chair";
			tempS = "";
		}

		//if (s == "tv")
		//{
		//	synS = "monitor";
		//}

		if (s == "diningtable")
		{
			synS = "officedesk";
		}

		if (s == "officedesk")
		{
			synS = "diningtable";
		}

		if ((!s.isEmpty() && m_modelCatNameList[i] == s)) 
		{
			idList.push_back(i);
		}
		//else if (usingSynset && !synS.isEmpty() && m_modelCatNameList[i].contains(synS) && !m_modelCatNameList[i].contains(s))
		else if (usingSynset && !synS.isEmpty() && m_modelCatNameList[i].contains(synS) && !(m_modelCatNameList[i] == tempS))
		{
			idList.push_back(i);
		}
	}

	return idList;
}

//void CScene::insertModel(QString modelFileName)
//{
//	CModel *m = new CModel();
//	m->loadModel(m_sceneFilePath + "/" + modelFileName + ".obj", m_metric, 0, m_sceneFormat);
//	m->setID(m_modelNum);
//	m->setJobName(m_jobName);
//
//	m->buildSuppPlane();
//	m->buildDisplayList();
//	m->prepareForIntersect();
//
//	m_modelList.push_back(m);
//	m_modelCatNameList.push_back(modelFileName);
//	m_modelNum++;
//
//	updateSeneAABB(m->getAABB());
//	m_relationGraph->InsertNode(0);
//}
//
//void CScene::insertModel(CModel *m)
//{
//	m->setID(m_modelNum);
//	m->setJustInserted(true);
//
//	m->buildSuppPlane();
//	m->buildDisplayList();
//	m->prepareForIntersect();
//
//	m_modelList.push_back(m);
//	m_modelCatNameList.push_back(m->getCatName());
//	m_modelNum++;
//
//	updateSeneAABB(m->getAABB());
//	m_relationGraph->InsertNode(0);
//}

//TO DO: fix support relationship after insert model
void CScene::buildSupportHierarchy()
{
	if (m_relationGraph->IsEmpty()) return;

	m_relationGraph->computeSupportParentForModels();
	std::vector<std::vector<int>> parentList = m_relationGraph->getSupportParentListForModels();

	// init all support to be -1
	foreach(CModel *m, m_modelList)
	{
		m->suppParentID = -1; 
		m->supportLevel = -10;
		m->suppChindrenList.clear();
	}

	// collect parent-child relationship
	for (int i = 0; i < m_relationGraph->Size(); i++)
	{
		std::vector<int> parentIDs = parentList[i];

		if (!parentIDs.empty())
		{
			int parentId = parentIDs[0];  // use the first parent in the list
			m_modelList[i]->suppParentID = parentId;
			m_modelList[parentId]->suppChindrenList.push_back(i);
		}
	}

	// init support level
	double minHeight = 1e6;
	std::vector<double> modelBottomHeightList(m_modelNum);

	int roomId = getRoomID();
	for (int i = 0; i < m_modelNum; i++)
	{
		if (i == roomId)
		{
			m_modelList[i]->supportLevel = -1;
		}

		if (m_modelList[i]->suppParentID == roomId)
		{
			// find models that are support by the floor
			m_modelList[i]->supportLevel = 0;
			CModel *currModel = m_modelList[i];
			setSupportChildrenLevel(currModel);
		}
	}

	// if support level is still not set, it may be hang on the wall
	// set support level of these models to be 1 and recursively set children's level
	for (int i = 0; i < m_modelNum; i++)
	{
		if (m_modelList[i]->supportLevel == -10) // still unset
		{
			m_modelList[i]->supportLevel = 0;

			setSupportChildrenLevel(m_modelList[i]);
		}
	}

	m_hasSupportHierarchy = true;

	//// update grid on support plane
	//for (int i = 0; i < m_modelNum; i++)
	//{
	//	int childNum = m_modelList[i]->suppChindrenList.size();
	//	if (childNum > 0)
	//	{
	//		for (int j = 0; j < childNum; j++)
	//		{
	//			int childID = m_modelList[i]->suppChindrenList[j];
	//		
	//			m_modelList[childID]->parentSuppPlaneID = findPlaneSuppPlaneID(childID, i);
	//		}			
	//	}
	//}
}
//
//int CScene::findPlaneSuppPlaneID(int childModelID, int parentModelID)
//{
//	double childModelBottomHeight = m_modelList[childModelID]->getOBBBottomHeight();
//
//	int suppPlaneNum = m_modelList[parentModelID]->getSuppPlaneNum();
//	double minDist = 1e6;
//	int closestPlaneID = -1;
//
//	for (int sid = 0; sid < suppPlaneNum; sid++)
//	{
//		double dist = abs(m_modelList[parentModelID]->getSuppPlane(sid)->GetZ() - childModelBottomHeight);
//
//		if (dist < minDist)
//		{
//			minDist = dist;
//			closestPlaneID = sid;
//		}
//	}
//
//	return closestPlaneID;
//}
//
void CScene::setSupportChildrenLevel(CModel *m)
{
	if (m->suppChindrenList.size() == 0)
	{
		return;
	}

	else
	{
		for (int i = 0; i < m->suppChindrenList.size(); i++)
		{
			m_modelList[m->suppChindrenList[i]]->supportLevel = m->supportLevel + 1;

			// set children's support level recursively
			setSupportChildrenLevel(m_modelList[m->suppChindrenList[i]]);
		}
	}
} 


void CScene::updateRelationGraph(int modelID)
{
	// scene graph should only be updated after model in inserted AND be transformed to new location
	m_relationGraph->updateGraph(modelID);
	//buildSupportHierarchy();  //update
}

void CScene::updateRelationGraph(int modelID, int suppModelID, int suppPlaneID)
{
	// only update graph linking with support model
	m_relationGraph->updateGraph(modelID, suppModelID);
//	updateSupportHierarchy(modelID, suppModelID, suppPlaneID);

}

int CScene::getRoomID()
{
	// if already know room id
	if (m_roomID != -1)
	{
		return m_roomID;
	}

	// set room ID
	for (int i = 0; i < m_modelList.size(); i++)
	{
		if (m_modelCatNameList[i].contains("room"))
		{
			m_roomID = i;
			break;
		}
	}

	return m_roomID;
}

MathLib::Vector3 CScene::getRoomFront()
{
	if (m_roomID != -1)
	{
		return m_modelList[m_roomID]->getFrontDir();;
	}

	else
	{
		int id = getRoomID();
		return m_modelList[id]->getFrontDir();
	}
}


//void CScene::computeTransformation(const std::vector<MathLib::Vector3> &source, const std::vector<MathLib::Vector3> &target, Eigen::Matrix4d &mat)
//{
//	Eigen::Matrix3Xd source_vertices;
//	Eigen::Matrix3Xd target_vertices;
//
//	source_vertices.resize(3, source.size());
//	target_vertices.resize(3, target.size());
//
//	for (int i = 0; i < source.size(); i++){
//		source_vertices(0, i) = (double)source[i].x;
//		source_vertices(1, i) = (double)source[i].y;
//		source_vertices(2, i) = (double)source[i].z;
//	}
//
//	for (int i = 0; i < target.size(); i++){
//		target_vertices(0, i) = (double)target[i].x;
//		target_vertices(1, i) = (double)target[i].y;
//		target_vertices(2, i) = (double)target[i].z;
//	}
//
//	Eigen::Affine3d affine = RigidMotionEstimator::point_to_point(source_vertices, target_vertices);
//	mat = affine.matrix().cast<double>();;
//}
//
//Eigen::Matrix4d  CScene::computeModelTransMat(int sourceModelId, QString tarFileName, QString sceneDBType)
//{
//	// load original model
//	CModel *tempModel = new CModel();
//	tempModel->loadModel(tarFileName, 1.0, 1, sceneDBType);
//
//	int sampleNum = 20;
//	CMesh *currMesh = m_modelList[sourceModelId]->getMesh();
//	CMesh *tempMesh = tempModel->getMesh();
//
//	sampleNum = std::min(sampleNum, currMesh->getVertsNum());
//
//	std::vector<int> randList = GetRandIntList(sampleNum, currMesh->getVertsNum());
//	std::vector<MathLib::Vector3> sourceVerts = tempMesh->getVerts(randList);
//	std::vector<MathLib::Vector3> targetVerts = currMesh->getVerts(randList);
//
//	int vid;
//	// find a vertex different from v0 to compute the scale
//	for (vid = 1; vid < sampleNum; vid++)
//	{
//		if (randList[vid] != randList[0])
//		{
//			std::abs((sourceVerts[0] - sourceVerts[vid]).magnitude()) > 0.01;
//			break;
//		}
//	}
//
//	double scaleFactor = (targetVerts[0] - targetVerts[vid]).magnitude() / (sourceVerts[0] - sourceVerts[vid]).magnitude();
//	
//	
//	
//	for (int i = 0; i < sourceVerts.size(); i++)
//	{
//		sourceVerts[i] = sourceVerts[i] * scaleFactor;
//	}
//
//	Eigen::Matrix4d eigenTransMat;
//	computeTransformation(sourceVerts, targetVerts, eigenTransMat);
//
//	for (int i = 0; i < 3; i++)
//	{
//		for (int j = 0; j < 3; j++)
//		{
//			eigenTransMat(i, j) *= scaleFactor;
//		}
//	}
//
//	delete tempModel;
//
//	return eigenTransMat;
//}

//void CScene::updateSupportHierarchy(int modelID, int suppModelID, int suppPlaneID)
//{
//	// recover grid value in previous support plane
//	int preParentID = m_modelList[modelID]->suppParentID;
//	if (preParentID!=-1)
//	{
//		int preSuppPlaneID = m_modelList[modelID]->parentSuppPlaneID;
//		if (preSuppPlaneID != -1)
//		{
//			SuppPlane *p = m_modelList[preParentID]->getSuppPlane(preSuppPlaneID);
//			p->recoverGrid(m_modelList[modelID]->suppGridPos);		
//		}
//	}
//
//	 //update support parent
//	if (!m_modelList[suppModelID]->isSupportChild(modelID))
//	{
//		m_modelList[modelID]->suppParentID = modelID;
//
//		m_modelList[suppModelID]->suppChindrenList.push_back(modelID);
//		m_modelList[modelID]->supportLevel = m_modelList[suppModelID]->supportLevel + 1;
//	}
//
//	// set new grid value using new obb location
//	if (m_modelList[suppModelID]->hasSuppPlane())
//	{
//		SuppPlane *p = m_modelList[suppModelID]->getSuppPlane(suppPlaneID);
//		m_modelList[modelID]->parentSuppPlaneID = suppPlaneID;
//
//		COBB obb = m_modelList[modelID]->getOBB();
//		std::vector<MathLib::Vector3> bottomCorners;
//		bottomCorners.push_back(obb.V(2));
//		bottomCorners.push_back(obb.V(6));
//		bottomCorners.push_back(obb.V(7));
//		bottomCorners.push_back(obb.V(3));
//
//		p->updateGrid(bottomCorners, 1);
//	}
//}



