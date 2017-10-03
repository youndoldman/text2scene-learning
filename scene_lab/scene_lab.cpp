#include "scene_lab.h"
#include "scene_lab_widget.h"
#include "modelDatabase.h"
#include "modelDBViewer_widget.h"
#include "RelationModelManager.h"
#include "RelationExtractor.h"
#include "../common/geometry/Scene.h"
#include "../t2scene/SceneSemGraph.h"
#include <set>
#include "engine.h"
#include <stdio.h>

Engine *matlabEngine;

//const QString SceneDBPath = "C:/Ruim/Graphics/T2S_MPC/SceneDB";

scene_lab::scene_lab(QObject *parent)
	: QObject(parent)
{
	m_widget = NULL;
	m_currScene = NULL;
	m_currSceneSemGraph = NULL;

	m_relationModelManager = NULL;
	m_relationExtractor = NULL;

	m_modelDB = NULL; 
	m_modelDBViewer_widget = NULL;

	loadParas();
}

scene_lab::~scene_lab()
{
	if (m_widget != NULL)
	{
		delete m_widget;
	}
}

void scene_lab::create_widget()
{
	m_widget = new scene_lab_widget(this);
	m_widget->show();
	m_widget->move(0, 200);

	std::cout << "SceneLab: widget created.\n";
}

void scene_lab::LoadScene()
{
	if (m_currScene != NULL)
	{
		delete m_currScene;
	}

	CScene *scene = new CScene();

	QString sceneFullName = m_widget->loadSceneName();

	QFile sceneFile(sceneFullName);
	QFileInfo sceneFileInfo(sceneFile.fileName());

	QString sceneFormat = sceneFileInfo.suffix();

	if (sceneFormat == "txt")
	{
		// only load scene mesh
		scene->loadStanfordScene(sceneFullName, 0, 0, 1);
		m_currScene = scene;

		if (m_modelDB == NULL)
		{
			m_modelDB = new ModelDatabase();
			m_modelDB->loadShapeNetSemTxt();
		}

		updateModelMetaInfoForScene(m_currScene);
	}
	else if (sceneFormat == "th")
	{
		scene->loadTsinghuaScene(sceneFullName, 0);
		m_currScene = scene;

		if (m_modelCatMap.empty())
		{
			loadModelCatsMap();
		}

		updateModelCatForScene(m_currScene);
	}
	else if (sceneFormat == "json")
	{
		scene->loadJsonScene(sceneFullName, 0);
		m_currScene = scene;
	}

	if (m_relationExtractor == NULL)
	{
		m_relationExtractor = new RelationExtractor(m_angleTh);
	}

	emit sceneLoaded();
}

void scene_lab::LoadSceneList(int metaDataOnly, int obbOnly, int meshAndOBB, int updateModelCat)
{
	if (m_relationExtractor == NULL)
	{
		m_relationExtractor = new RelationExtractor(m_angleTh);
	}

	if (!m_sceneList.empty())
	{
		for (int i = 0; i < m_sceneList.size(); i++)
		{
			if (m_sceneList[i]!=NULL)
			{
				delete m_sceneList[i];
			}
		}
	}

	m_sceneList.clear();

	loadSceneFileNamesFromListFile(m_loadedSceneFileNames);

	if (m_sceneDBType != "tsinghua")
	{
		if (m_modelDB == NULL)
		{
			m_modelDB = new ModelDatabase();
			m_modelDB->loadShapeNetSemTxt();
		}
	}
	else
	{
		if (m_modelCatMap.empty())
		{
			loadModelCatsMap();
		}
	}
	
	// load stanford or scenenn scenes
	for (int i = 0; i < m_loadedSceneFileNames[0].size(); i++)
	{
		CScene *scene = new CScene();
		scene->loadStanfordScene(m_loadedSceneFileNames[0][i], metaDataOnly, obbOnly, meshAndOBB);

		updateModelMetaInfoForScene(scene);
		m_sceneList.push_back(scene);
	}

	// load tsinghua scenes
	for (int i = 0; i < m_loadedSceneFileNames[1].size(); i++)
	{
		CScene *scene = new CScene();
		scene->loadTsinghuaScene(m_loadedSceneFileNames[1][i], obbOnly);

		if (updateModelCat)
		{
			updateModelCatForScene(scene);
		}

		m_sceneList.push_back(scene);
	}
}

void scene_lab::loadModelCatsMap()
{
	QString currPath = QDir::currentPath();
	QString modelCatFileName = currPath + "/ModelCategoryMap.txt";
	QFile modelCatFile(modelCatFileName);

	if (!modelCatFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		std::cout << "Cannot open model category map file\n";
	}

	QTextStream ifs(&modelCatFile);

	while (!ifs.atEnd())
	{
		QString currLine = ifs.readLine();
		QStringList catNames = currLine.split(",");
	
		// save category if it does not exist in the map
		if (m_modelCatMap.find(catNames[0]) == m_modelCatMap.end())
		{
			m_modelCatMap[catNames[0]] = catNames[1];
		}
	}

	modelCatFile.close();
}

void scene_lab::updateModelCatForScene(CScene *s)
{
	for (int i=0; i < s->getModelNum(); i++)
	{
		CModel *currModel = s->getModel(i);
		QString currModelCat = currModel->getCatName();

		if (m_modelCatMap.count(currModelCat))
		{
			currModel->setCatName(m_modelCatMap[currModelCat]);
		}
	}
}

void scene_lab::ExtractModelCatsFromSceneList()
{
	loadParas();
	LoadSceneList(1, 1, 0, 0);  // load scenes without updating model cats

	int stanfordSceneNum = m_loadedSceneFileNames[0].size();
	int tsinghuaSceneNum = m_loadedSceneFileNames[1].size();

	// collect model categories for stanford scenes

	std::set<QString> allModelNameStrings;

	//for (int i = 0; i < m_sceneList.size(); i++)
	for (int i = 0; i < stanfordSceneNum; i++)
	{
		for (int j = 0; j <  m_sceneList[i]->getModelNum(); j++)
		{
			allModelNameStrings.insert(m_sceneList[i]->getModelNameString(j));
		}
	}

	if (!allModelNameStrings.empty())
	{
		// collect corrected model category
		QString currPath = QDir::currentPath();
		QString correctedModelCatFileName = currPath + "/stanford_NameCatMap_corrected.txt";

		QFile outFile(correctedModelCatFileName);
		QTextStream ofs(&outFile);

		if (!outFile.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
		{
			std::cout << "SceneLab: cannot open model category name map file.\n";
			return;
		}

		std::set<QString> allModelCats;
		for (auto it = allModelNameStrings.begin(); it != allModelNameStrings.end(); it++)
		{
			if (m_modelDB != NULL && m_modelDB->dbMetaModels.count(*it))
			{
				auto metaIt = m_modelDB->dbMetaModels.find(*it);
				DBMetaModel *m = metaIt->second;
				QString modelCat = m->getProcessedCatName();
				allModelCats.insert(modelCat);

				ofs << m->getIdStr() << "," << modelCat << "\n";
			}
		}

		outFile.close();

		QString modelCatFileName = currPath + "/stanford_model_category_corrected.txt";
		outFile.setFileName(modelCatFileName);

		if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cout << "SceneLab: cannot open model category file.\n"; 
			return;
		}

		for (auto it = allModelCats.begin(); it != allModelCats.end(); it++)
		{
			ofs << *it << "\n";
		}

		outFile.close();
	}

	// collect model categories for tsinghua scenes
	std::set<QString> allModelCats;
	for (int i = stanfordSceneNum; i < stanfordSceneNum + tsinghuaSceneNum; i++)
	{
		for (int j = 0; j < m_sceneList[i]->getModelNum(); j++)
		{
			allModelCats.insert(m_sceneList[i]->getModelCatName(j));  // directly get model category name for tsinghua scenes
		}
	}

	if (!allModelCats.empty())
	{
		QString currPath = QDir::currentPath();
		QString modelCatFileName = currPath + "/tsinghua_model_category.txt";

		QFile outFile(modelCatFileName);
		QTextStream ofs(&outFile);

		if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cout << "\tCannot open file " << modelCatFileName.toStdString() << std::endl;
		}

		for (auto it = allModelCats.begin(); it != allModelCats.end(); it++)
		{
			ofs << *it << "\n";
		}

		outFile.close();
	}

	std::cout << "\nSceneLab: model category saved.\n";
}

// update cat name, front dir, up dir for model
void scene_lab::updateModelMetaInfoForScene(CScene *s)
{
	if (s!= NULL)
	{
		for (int i = 0; i < s->getModelNum(); i++)
		{
			QString modelNameString = s->getModelNameString(i);

			if (m_modelDB->dbMetaModels.count(modelNameString))
			{
				MathLib::Vector3 frontDir = m_modelDB->dbMetaModels[modelNameString]->frontDir;
				s->updateModelFrontDir(i, frontDir);

				MathLib::Vector3 upDir = m_modelDB->dbMetaModels[modelNameString]->upDir;
				s->updateModelUpDir(i, upDir);

				QString catName = m_modelDB->dbMetaModels[modelNameString]->getProcessedCatName();
				s->updateModelCat(i, catName);
			}

			if (modelNameString.contains("room"))
			{
				s->updateModelCat(i, "room");
			}
		}
	}
}

void scene_lab::BuildOBBForSceneList()
{
	std::vector<QStringList> loadedSceneFileNames;
	loadSceneFileNamesFromListFile(loadedSceneFileNames);

	// load stanford or scenenn scenes and build the obb
	for (int i = 0; i < loadedSceneFileNames[0].size(); i++)
	{
		CScene *scene = new CScene();
		scene->loadStanfordScene(loadedSceneFileNames[0][i], 0, 0, 0);
	}

	// load tsinghua scenes and build the obb
	for (int i = 0; i < loadedSceneFileNames[1].size(); i++)
	{
		CScene *scene = new CScene();
		scene->loadTsinghuaScene(loadedSceneFileNames[1][i], 0);

		if (scene!=NULL)
		{
			delete scene;
		}
	}
}

void scene_lab::destroy_widget()
{
	if (m_widget != NULL)
	{
		delete m_widget;
	}
}

void scene_lab::setDrawArea(Starlab::DrawArea *drawArea)
{
	m_drawArea = drawArea;
}

void scene_lab::loadParas()
{
	QString currPath = QDir::currentPath();
	std::vector<std::string> paraLines = GetFileLines(currPath.toStdString() +"/paras.txt", 3);

	for (int i=0;  i<paraLines.size(); i++)
	{
		if (paraLines[i][0]!='#')
		{
			if (paraLines[i].find("SceneDBType=") != std::string::npos)
			{
				m_sceneDBType = toQString(PartitionString(paraLines[i], "SceneDBType=")[0]);
				continue;
			}

			if (paraLines[i].find("LocalSceneDBPath=") != std::string::npos)
			{
				m_localSceneDBPath = toQString(PartitionString(paraLines[i], "LocalSceneDBPath=")[0]);
				continue;
			}

			if (paraLines[i].find("AngleThreshold=") != std::string::npos)
			{
				m_angleTh = StringToFloatList(paraLines[i], "AngleThreshold=")[0];
				continue;
			}
		}
	}
}

void scene_lab::loadSceneFileNamesFromListFile(std::vector<QStringList> &loadedSceneFileNames)
{
	// load scene list file
	QString currPath = QDir::currentPath();
	QString sceneListFileName = currPath + QString("/scene_list_%1.txt").arg(m_sceneDBType);


	QFile inFile(sceneListFileName);
	QTextStream ifs(&inFile);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		std::cout << "SceneLab: cannot open scene list file.\n";
		return;
	}

	QStringList sceneFolder;
	sceneFolder.push_back(m_localSceneDBPath + "/StanfordSceneDB/scenes");
	sceneFolder.push_back(m_localSceneDBPath + "/TsinghuaSceneDB/scenes");

	QString currLine = ifs.readLine();

	if (currLine.contains("SceneNum"))
	{
		std::vector<int> sceneNum = StringToIntegerList(currLine.toStdString(), "SceneNum ");

		if (sceneNum.size() != 2)
		{
			std::cout << "Wrong file format for scene list: need two scene numbers";
			return;
		}

		QStringList currSceneNames;
		// load stanford or scenenn scene names
		for (int i = 0; i < sceneNum[0]; i++)
		{
			QString sceneName = ifs.readLine();
			QString filename = sceneFolder[0] + "/" + sceneName + ".txt";
			currSceneNames.push_back(filename);
		}

		loadedSceneFileNames.push_back(currSceneNames);

		// load tsinghua scenes
		currSceneNames.clear();
		for (int i = 0; i < sceneNum[1]; i++)
		{
			QString sceneName = ifs.readLine();
			QString filename = sceneFolder[1] + "/" + sceneName + ".th";
			currSceneNames.push_back(filename);
		}

		loadedSceneFileNames.push_back(currSceneNames);
	}
}

void scene_lab::updateSceneRenderingOptions()
{
	bool showModelOBB = m_widget->ui->showOBBCheckBox->isChecked();
	bool showSuppGraph = m_widget->ui->showGraphCheckBox->isChecked();
	bool showFrontDir = m_widget->ui->showFrontDirCheckBox->isChecked();
	bool showSuppPlane = m_widget->ui->showSuppPlaneCheckBox->isChecked();
	
	if (m_currScene != NULL)
	{
		m_currScene->setShowModelOBB(showModelOBB);
		m_currScene->setShowSceneGraph(showSuppGraph);
		m_currScene->setShowSuppPlane(showSuppPlane);
		m_currScene->setShowModelFrontDir(showFrontDir);

	}

	if (m_modelDBViewer_widget !=NULL)
	{
		m_modelDBViewer_widget->updateRenderingOptions(showModelOBB, showFrontDir, showSuppPlane);
	}

	emit sceneRenderingUpdated();
}

void scene_lab::create_modelDBViewer_widget()
{
	if (m_modelDB == NULL)
	{
		m_modelDB = new ModelDatabase();
		m_modelDB->loadShapeNetSemTxt();
	}

	m_modelDBViewer_widget = new ModelDBViewer_widget(m_modelDB);
	m_modelDBViewer_widget->show();
}

void scene_lab::destory_modelDBViewer_widget()
{

}

#define BUFSIZE 1000
void scene_lab::testMatlab()
{
	// 1. Open MATLAB engine
	matlabEngine = engOpen(NULL);
	if (matlabEngine == NULL) {
		printf("Can't start MATLAB engine!!!\n");
		exit(-1);
	}

	// 2. Call MATLAB engine
	{
		// 2.1. Pre-work: capture MATLAB output. Ensure the buffer is always NULL terminated.
		char buffer[BUFSIZE + 1];
		buffer[BUFSIZE] = '\0';
		engOutputBuffer(matlabEngine, buffer, BUFSIZE);
		engEvalString(matlabEngine, "clc;clear;"); // clear all variables (optional)

											 // 2.2. Setup inputs: a, b
		mxArray *a = mxCreateDoubleScalar(2); // assume a=2
		mxArray *b = mxCreateDoubleScalar(3); // assume b=3
		engPutVariable(matlabEngine, "a", a); // put into matlab
		engPutVariable(matlabEngine, "b", b); // put into matlab

										// 2.3. Call MATLAB
		engEvalString(matlabEngine, "cd \'C:\\Ruim\\Graphics\\T2S_MPC\\text2scene-learning\\scene_lab'");
		engEvalString(matlabEngine, "[y, z] = myadd2(a, b);");
		printf("%s\n", buffer); // get error messages or prints (optional)

							 // 2.4. Get result: y, z
		mxArray *y = engGetVariable(matlabEngine, "y");
		mxArray *z = engGetVariable(matlabEngine, "z");
		double y_res = mxGetScalar(y);
		double z_res = mxGetScalar(z);
		printf("y=%f\nz=%f\n", y_res, z_res);

		// 2.5. Release (to all mxArray)
		mxDestroyArray(a);
		mxDestroyArray(b);
		mxDestroyArray(y);
		mxDestroyArray(z);
	}

	// 3. Close MATLAB engine
	engClose(matlabEngine);
}

void scene_lab::ScreenShotForCurrScene()
{
	if (m_currScene != NULL)
	{
		// reset camera
		m_drawArea->camera()->setOrientation(0, -MathLib::ML_PI_2);
		m_drawArea->camera()->setViewDirection(qglviewer::Vec(-1, 1, -1));
		m_drawArea->camera()->showEntireScene();
		m_drawArea->updateGL();

		QString sceneFilePath = m_currScene->getFilePath();
		QString sceneName = m_currScene->getSceneName();
		m_drawArea->saveSnapshot(sceneFilePath + "/" + sceneName + ".png");
		std::cout << "Screenshot for " << sceneName.toStdString() << " saved\n";

	}
}

void scene_lab::ScreenShotForSceneList()
{
	loadParas();
	LoadSceneList(0,0,1);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];

		//emit sceneRenderingUpdated();
		emit sceneLoaded();

		ScreenShotForCurrScene();
	}
}

void scene_lab::BuildSemGraphForCurrentScene()
{
	if (m_currScene == NULL)
	{
		Simple_Message_Box("No scene is loaded");
		return;
	}

	if (m_currSceneSemGraph!=NULL)
	{
		delete m_currSceneSemGraph;
	}

	m_currSceneSemGraph = new SceneSemGraph(m_currScene, m_modelDB, m_relationExtractor);
	m_currSceneSemGraph->generateGraph();
	m_currSceneSemGraph->saveGraph();

}

void scene_lab::BuildSemGraphForSceneList()
{
	loadParas();

	// only load meta data (stanford and scenenn) and obb (tsinghua)
	LoadSceneList(1,1,0);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		BuildSemGraphForCurrentScene();
	}

	std::cout << "\nSceneLab: all scene semantic graph generated.\n";
}

void scene_lab::BuildRelationGraphForCurrentScene()
{
	if (m_currScene == NULL)
	{
		Simple_Message_Box("No scene is loaded");
		return;
	}

	m_currScene->buildRelationGraph();
}

void scene_lab::BuildRelationGraphForSceneList()
{
	loadParas();

	if (m_sceneDBType == "stanford")
	{
		LoadSceneList(0, 1);
	}
	else
	{
		LoadSceneList(0, 0, 1);
	}

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];

		BuildRelationGraphForCurrentScene();
	}

	std::cout << "\nSceneLab: all scene relation graph generated.\n";
}

void scene_lab::ExtractMetaFileForSceneList()
{
	// collect model meta info for current scene list. (subset of the whole shapenetsem meta file)

	std::set<QString> allModelNameStrings;

	// load scene list file
	QString currPath = QDir::currentPath();
	QString sceneListFileName = currPath + "/scene_list.txt";
	QString sceneDBPath = "C:/Ruim/Graphics/T2S_MPC/SceneDB/StanfordSceneDB/scenes";

	QFile inFile(sceneListFileName);
	QTextStream ifs(&inFile);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		std::cout << "SceneLab: cannot open scene list file.\n";
		return;
	}

	QString currLine = ifs.readLine();

	if (currLine.contains("SceneNum"))
	{
		int sceneNum = StringToIntegerList(currLine.toStdString(), "StanfordSceneDatabase ")[0];

		for (int i = 0; i < sceneNum; i++)
		{
			QString sceneName = ifs.readLine();

			if (m_currScene != NULL)
			{
				delete m_currScene;
			}

			CScene *scene = new CScene();
			QString filename = sceneDBPath + "/" + sceneName + ".txt";
			scene->loadStanfordScene(filename, 1, 0, 0);
			m_currScene = scene;
			
			for (int mi = 0; mi < m_currScene->getModelNum(); mi++)
			{
				allModelNameStrings.insert(m_currScene->getModelNameString(mi));
			}			
		}
	}

	QString modelMetaInfoFileName = currPath + "/scene_list_model_info.txt";

	QFile outFile(modelMetaInfoFileName);
	QTextStream ofs(&outFile);

	if (!outFile.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
	{
		std::cout << "SceneLab: cannot open model meta info file.\n";
		return;
	}

	if (m_modelDB == NULL)
	{
		m_modelDB = new ModelDatabase();
		m_modelDB->loadShapeNetSemTxt();
	}

	for (auto it = allModelNameStrings.begin(); it != allModelNameStrings.end(); it++)
	{
		if (m_modelDB->dbMetaModels.count(*it))
		{
			auto metaIt = m_modelDB->dbMetaModels.find(*it);
			DBMetaModel *m = metaIt->second;
			ofs << QString(m_modelDB->modelMetaInfoStrings[m->dbID].c_str()) << "\n";				
		}
	}

	outFile.close();

	std::cout << "\nSceneLab: model meta info saved.\n";
}

void scene_lab::BuildRelativeRelationModels()
{
	//testMatlab();

	loadParas();
	// load metadata
	LoadSceneList(1);

	if (m_relationModelManager!=NULL)
	{
		delete m_relationModelManager;
	}

	m_relationModelManager = new RelationModelManager(m_relationExtractor);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		m_relationModelManager->updateCurrScene(m_currScene);
		m_relationModelManager->loadRelativePosFromCurrScene();
	}

	m_relationModelManager->buildRelativeRelationModels();
	m_relationModelManager->saveRelativeRelationModels(m_localSceneDBPath, m_sceneDBType);
}

void scene_lab::BuildPairwiseRelationModels()
{
	loadParas();
	LoadSceneList(1);

	if (m_relationModelManager != NULL)
	{
		delete m_relationModelManager;
	}

	m_relationModelManager = new RelationModelManager(m_relationExtractor);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		m_currScene->loadSSG();

		m_relationModelManager->updateCurrScene(m_currScene);
		m_relationModelManager->loadRelativePosFromCurrScene();
		m_relationModelManager->collectPairwiseInstanceFromCurrScene();
	}

	m_relationModelManager->buildPairwiseRelationModels();
	m_relationModelManager->computeSimForPairwiseModels(m_relationModelManager->m_pairwiseRelModels, m_relationModelManager->m_pairRelModelKeys, m_sceneList, false, m_localSceneDBPath);

	m_relationModelManager->savePairwiseRelationModels(m_localSceneDBPath, m_sceneDBType);
	m_relationModelManager->savePairwiseModelSim(m_localSceneDBPath, m_sceneDBType);
}

void scene_lab::BuildGroupRelationModels()
{
	loadParas();
	LoadSceneList(1);

	if (m_relationModelManager != NULL)
	{
		delete m_relationModelManager;
	}

	m_relationModelManager = new RelationModelManager(m_relationExtractor);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		m_currScene->loadSSG();

		m_relationModelManager->updateCurrScene(m_currScene);
		m_relationModelManager->loadRelativePosFromCurrScene();
		m_relationModelManager->collectGroupInstanceFromCurrScene();
	}

	m_relationModelManager->buildGroupRelationModels();
	m_relationModelManager->computeSimForPairModelInGroup(m_sceneList);

	m_relationModelManager->saveGroupRelationModels(m_localSceneDBPath, m_sceneDBType);
	m_relationModelManager->saveGroupModelSim(m_localSceneDBPath, m_sceneDBType);

	m_relationModelManager->saveCoOccurInGroupModels(m_localSceneDBPath, m_sceneDBType);
}

void scene_lab::BatchBuildModelsForList()
{
	uint64 startTime = GetTimeMs64();

	loadParas();
	// load metadata
	LoadSceneList(1);

	if (m_relationModelManager != NULL)
	{
		delete m_relationModelManager;
	}

	m_relationModelManager = new RelationModelManager(m_relationExtractor);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		m_currScene->loadSSG();

		m_relationModelManager->updateCurrScene(m_currScene);
		m_relationModelManager->loadRelativePosFromCurrScene();

		m_relationModelManager->collectPairwiseInstanceFromCurrScene();
		m_relationModelManager->collectGroupInstanceFromCurrScene();
		m_relationModelManager->collectSupportRelationInCurrentScene();
	}

	// relative
	m_relationModelManager->buildRelativeRelationModels();
	m_relationModelManager->saveRelativeRelationModels(m_localSceneDBPath, m_sceneDBType);

	// pairwise
	m_relationModelManager->buildPairwiseRelationModels();
	m_relationModelManager->computeSimForPairwiseModels(m_relationModelManager->m_pairwiseRelModels, m_relationModelManager->m_pairRelModelKeys, m_sceneList, false, m_localSceneDBPath);

	m_relationModelManager->savePairwiseRelationModels(m_localSceneDBPath, m_sceneDBType);
	m_relationModelManager->savePairwiseModelSim(m_localSceneDBPath, m_sceneDBType);

	// group
	m_relationModelManager->buildGroupRelationModels();
	m_relationModelManager->computeSimForPairModelInGroup(m_sceneList);

	m_relationModelManager->saveGroupRelationModels(m_localSceneDBPath, m_sceneDBType);
	m_relationModelManager->saveGroupModelSim(m_localSceneDBPath, m_sceneDBType);

	// support
	m_relationModelManager->buildSupportRelationModels();
	m_relationModelManager->saveSupportRelationModels(m_localSceneDBPath, m_sceneDBType);

	uint64 endTime = GetTimeMs64();
	qDebug() << QString("Done in %1 seconds").arg((endTime-startTime)/1000);
}

void scene_lab::ComputeBBAlignMatForSceneList()
{
	loadParas();

	// load mesh without OBB
	LoadSceneList(0, 0, 0);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		// update front dir for alignment
		m_currScene = m_sceneList[i];
		m_currScene->computeModelBBAlignMat();

		qDebug() << "SceneLab: bounding box alignment matrix saved for " << m_currScene->getSceneName();
	}
}

void scene_lab::ExtractRelPosForSceneList()
{
	loadParas();

	// load OBB only
	LoadSceneList(0, 1, 0);

	if (m_relationModelManager != NULL)
	{
		delete m_relationModelManager;
	}

	m_relationModelManager = new RelationModelManager(m_relationExtractor);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		m_relationModelManager->updateCurrScene(m_currScene);

		m_relationModelManager->collectRelativePosInCurrScene();

		qDebug() << "SceneLab: relative position saved for " << m_currScene->getSceneName();
	}
}

void scene_lab::ExtractSuppProbForSceneList()
{
	loadParas();
	LoadSceneList(1);

	if (m_relationModelManager != NULL)
	{
		delete m_relationModelManager;
	}

	m_relationModelManager = new RelationModelManager(m_relationExtractor);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		m_currScene->loadSSG();

		m_relationModelManager->updateCurrScene(m_currScene);
		m_relationModelManager->collectSupportRelationInCurrentScene();

		m_relationModelManager->collectCoOccInCurrentScene();
	}

	m_relationModelManager->buildSupportRelationModels();
	m_relationModelManager->saveSupportRelationModels(m_localSceneDBPath, m_sceneDBType);

	for (int i = 0; i < m_sceneList.size(); i++)
	{
		m_currScene = m_sceneList[i];
		m_relationModelManager->updateCurrScene(m_currScene);
		m_relationModelManager->collectSupportRelationInCurrentScene();

		m_relationModelManager->addOccToCoOccFromCurrentScene();
	}

	m_relationModelManager->computeOccToCoccOnSameParent();
	m_relationModelManager->saveCoOccurOnParentModels(m_localSceneDBPath, m_sceneDBType);
}

