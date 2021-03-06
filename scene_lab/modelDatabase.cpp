#include "modelDatabase.h"
#include "category.h"
#include "../common/geometry/CModel.h"
#include "../common/utilities/utility.h"
#include <QDir>
#include <QFile>
#include <QTextStream>

ModelDatabase::ModelDatabase()
{
	//// load model DB path from file
	//QString currPath = QDir::currentPath();
	//std::string dbPathFileName = currPath.toStdString() + "/ModelDBPath.txt";

	//auto lines = GetFileLines(dbPathFileName, 0);

	//for (int i = 0; i < lines.size();i++)
	//{
	//	if (QString(lines[i][0]) != "%")
	//	{
	//		m_dbPath = QString(lines[i].c_str());
	//		m_dbMetaFileType = QString(lines[i+1].c_str());
	//		break;
	//	}
	//}

	//m_parentCatNum = 0;
	//m_modelNum = 0;
}


ModelDatabase::ModelDatabase(const QString &projectPath, int dBType)
{
	m_projectPath = projectPath;

	if (dBType == ModelDBType::ShapeNetDB)
	{
		std::string dbPathFileName = m_projectPath.toStdString() + "/paras/ShapeNetModelDBPath.txt";

		auto lines = GetFileLines(dbPathFileName, 0);

		for (int i = 0; i < lines.size(); i++)
		{
			if (QString(lines[i][0]) != "%")
			{
				m_dbPath = QString(lines[i].c_str());
				m_dbMetaFileType = QString(lines[i + 1].c_str());
				break;
			}
		}
	}

	if (dBType == ModelDBType::SunCGDB)
	{

	}

	m_parentCatNum = 0;
	m_modelNum = 0;
}

ModelDatabase::~ModelDatabase()
{

}

void ModelDatabase::loadModelTsv(const QString &modelsTsvFile)
{
	auto lines = GetFileLines(modelsTsvFile.toStdString(), 3);

	for (const std::string &line : lines)
	{
		auto parts = PartitionString(line, "\t");

		QString modelIdStr = QString(parts[0].c_str());
		DBMetaModel *cm = new DBMetaModel(modelIdStr);			
		dbMetaModels[modelIdStr] = cm;

		if (parts.size() >= 2 && parts[1].size() > 0)
		{
			QString catName = QString(parts[1].c_str());			

			if (parts.size() >= 3)
			{
				catName = QString(parts[2].c_str()); // overwrite category
			}

			catName = catName.toLower();

			// remove "_" in catName
			catName.remove("_");

			cm->setCatName(catName);

			if (dbCategories.count(catName) == 0)
			{
				Category *cat = new Category(catName);
				dbCategories[catName] = cat;
			}
		}

		dbCategories[cm->getCatName()]->addInstance(cm);
	}
}

void ModelDatabase::readModelScaleFile(const QString &filename)
{
	auto lines = GetFileLines(filename.toStdString(), 0);

	//std::map<std::string, float> modelScaleById;
	for (int lineIndex = 1; lineIndex < lines.size(); lineIndex++)
	{
		//wss.383955142f43ca0b4063d41fae33f144, 0.026278285548332416, 1, sceneScales, , diagonal
		auto parts = PartitionString(lines[lineIndex], ",");
		if (parts.size() >= 2)
		{
			QString modelIdStr = QString(parts[0].c_str());
			if (dbMetaModels.count(modelIdStr) > 0)
			{
				dbMetaModels[modelIdStr]->setScale(StringToFloat(parts[1]));
			}
		}
	}
}

bool ModelDatabase::loadSceneSpecifiedModelFile(const QString &filename, QStringList &objNameStrings, bool isSharedModelFile)
{
	QFile inFile(filename);
	QTextStream ifs(&inFile);

	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) return false;

	std::cout << "ModelDatabase: loading ShapeNetSem meta file...";

	QString currLine;

	while (!ifs.atEnd())
	{
		currLine = ifs.readLine();
		QStringList lineparts = currLine.split("_");

		QString modelIdStr = lineparts[lineparts.size() - 1];
		DBMetaModel *cm = new DBMetaModel(modelIdStr);
		dbMetaModels[modelIdStr] = cm;

		QString catName = lineparts[0];

		if (lineparts.size() > 1)
		{
			objNameStrings.push_back(lineparts[1]);
		}		

		if (catName.contains("room"))
		{
			catName = "room";
		}

		else if (catName[catName.size() - 1].isDigit())   // get rid of the digit in the object category 
		{
			catName = catName.left(catName.size() - 1);
		}

		if (catName == "openbook")
		{
			catName = "book";
		}

		if (isSharedModelFile)
		{
			catName = QString("shared_" + catName);
		}
		
		cm->setCatName(catName);

		if (dbCategories.count(catName) == 0)
		{
			Category *cat = new Category(catName);
			dbCategories[catName] = cat;
		}

		dbCategories[cm->getCatName()]->addInstance(cm);
	}

	inFile.close();

	std::cout << "done\n";
	return true;
}

CModel* ModelDatabase::getModelByCat(const QString &catName)
{
	Category* currCat = getCategory(catName);
	DBMetaModel* candiModel = currCat->sampleInstance();

	QString modelIdStr = candiModel->getIdStr();
	
	CModel *m = getModelById(modelIdStr);

	return m;
}

CModel* ModelDatabase::getModelById(QString idStr)
{
	CModel *m = new CModel();
	
	if (idStr.contains("wss."))
	{
		idStr = idStr.remove("wss.");
	}
	//m->loadModel(m_dbPath + "/wss.models/models/" + idStr +".obj", candiModel->getScale());

	m->loadModel(m_dbPath + "/" + idStr + ".obj", 1.0, 0, 0, 1);  // load model and cat name in .anno file

	//QString catName = getModelCat("wss." + idStr);
	//m->setCatName(catName);   // set cat name in DB csv file
	
	return m; 
}

Category* ModelDatabase::getCategory(QString catName)
{
	//if (catName == "mouse")
	//{
	//	catName = QString("computermouse");
	//}

	//if (catName == "wine glass")
	//{
	//	catName = QString("drinkingutensil");
	//}

	if (dbCategories.count(catName) == 0)
	{
		QString synName; 

		if (catName == "officechair")
		{
			synName = "diningchair";
		}

		if (catName == "diningchair")
		{
			synName = "officechair";
		}

		if (catName == "diningtable")
		{
			synName = "officedesk";
		}

		if (catName == "officedesk")
		{
			synName = "diningtable";
		}

		return dbCategories.find(synName)->second;
	}

	return dbCategories.find(catName)->second;
}

bool ModelDatabase::isCatInDB(QString catname)
{
	if (dbCategories.count(catname) == 0)
	{
		if (catname == "coffeetable")
		{
			catname = "sidetable";
			return dbCategories.count(catname) != 0;
		}

		if (catname=="officechair")
		{
			catname = "diningchair";
			return dbCategories.count(catname) != 0;
		}

		return false;
	}
	else
		return true;
}

// add models to annotated models: 1. create modelname.arv under interaction map
void ModelDatabase::extractScaledAnnoModels()
{
	int cutPos = m_dbPath.lastIndexOf("/");
	QString dbParentPath = m_dbPath.left(cutPos);

	QString interFolder = QString(dbParentPath + "/interaction_maps");

	QDir interFolderDir(interFolder);

	if (interFolderDir.exists())
	{
		QStringList fileList = interFolderDir.entryList();
		QStringList modelNames;

		foreach(QString s, fileList)
		{
			if (s.contains(".arv"))
			{
				modelNames.push_back(s.remove(".arv"));
			}
		}

		QString selModelFileName = QString(dbParentPath + "/ann_model_categories.tsv");
		QString selModelScaleName = QString(dbParentPath + "/ann_model_scales.csv");

		QFile modelFile(selModelFileName);
		QTextStream ofs_model(&modelFile);

		QFile scaleFile(selModelScaleName);
		QTextStream ofs_scale(&scaleFile);
		

		if (!modelFile.open(QIODevice::ReadWrite | QIODevice::Text)) return;
		if (!scaleFile.open(QIODevice::ReadWrite | QIODevice::Text)) return;


		foreach(QString s, modelNames)
		{
			DBMetaModel *m = dbMetaModels[s];

			if (m!=NULL)    // skip those with no category in model_categories.tsv, e.g. room
			{
				ofs_model << m->getIdStr() << "\t" << m->getCatName() << "\n";
				ofs_scale << m->getIdStr() << "," << m->getScale() << "\n";
			}
		}

		modelFile.close();
		scaleFile.close();

		foreach(QString s, modelNames)
		{
			QString modelIdStr = s;

			if (modelIdStr.contains("wss."))
			{
				modelIdStr = modelIdStr.remove("wss.");
			}

			QString modelFileName = QString(dbParentPath + "/models_scaled/" + modelIdStr + ".obj");

			QFile modelFile(modelFileName);
			if (!modelFile.exists())
			{
				CModel *m = new CModel();

				if (!modelIdStr.contains("room"))   // do not scale room, since no scale info in scale file
				{
					m->loadModel(dbParentPath + "/wss.models/models/" + modelIdStr + ".obj", dbMetaModels[s]->getScale());
					m->saveModel(dbParentPath + "/models_scaled/" + modelIdStr + ".obj");
				}
			}
		}

		Simple_Message_Box("Extract scaled models done!");
	}

	else
	{
		Simple_Message_Box("Cannot find interaction Map folder");
	}
}

// extract annotated model
void ModelDatabase::extractModelWithTexture()
{
	int cutPos = m_dbPath.lastIndexOf("/");
	QString dbParentPath = m_dbPath.left(cutPos);

	QString interFolder = QString(dbParentPath + "/interaction_maps");

	QDir interFolderDir(interFolder);

	if (interFolderDir.exists())
	{
		QStringList fileList = interFolderDir.entryList();
		QStringList modelNames;

		foreach(QString s, fileList)
		{
			if (s.contains(".arv"))
			{
				modelNames.push_back(s.remove(".arv"));
			}
		}

		foreach(QString s, modelNames)
		{
			QString modelIdStr = s;

			if (modelIdStr.contains("wss."))
			{
				modelIdStr = modelIdStr.remove("wss.");
			}

			QString tarModelFileName = QString(dbParentPath + "/models_with_texture/" + modelIdStr + ".obj");
			QString originModelFileName = QString(dbParentPath + "/wss.models/models/" + modelIdStr + ".obj");

			QString tarMtlFileName = QString(dbParentPath + "/models_with_texture/" + modelIdStr + ".mtl");
			QString originMtlFileName = QString(dbParentPath + "/wss.models/models/" + modelIdStr + ".mtl");

			QFile tarModelFile(tarModelFileName);

			if (!tarModelFile.exists())  // skip file that has been copied
			{
				QFile::copy(originModelFileName, tarModelFileName);

				QFile originMtlFile(originMtlFileName);

				if (originMtlFile.open(QIODevice::ReadOnly | QIODevice::Text))
				{
					QFile::copy(originMtlFileName, tarMtlFileName);
					QTextStream ofs_originMtl(&originMtlFile);

					QString currLine;

					while (!ofs_originMtl.atEnd())
					{
						currLine = ofs_originMtl.readLine();

						if (currLine.contains(".jpg"))
						{
							QStringList strList = currLine.split(" ");
							QString imgName = strList[1];   //e.g. map_Ka 0d10c44f0bd46c38.jpg

							QString tarImgFileName = dbParentPath + "/models_with_texture/" + "/" + imgName;
							QString originImgFileName = dbParentPath + "/wss.models/models/" + "/" + imgName;

							QFile::copy(originImgFileName, tarImgFileName);
						}
					}

					originMtlFile.close();
				}
			}	
		}

		Simple_Message_Box("Extract textured models done!");
	}
}

int ModelDatabase::getModelNum()
{
	return dbMetaModels.size();
}

QString ModelDatabase::getModelIdStr(int id)
{
	std::map<QString, DBMetaModel*>::iterator it = dbMetaModels.begin();

	std::advance(it, id);

	return it->first;
}

QString ModelDatabase::getModelCat(const QString &idStr)
{
	DBMetaModel *cm = dbMetaModels[idStr];
	return cm->getCatName();
}

QString ModelDatabase::getUpdatedModelCat(const QString &catName, const QString &modelIdStr)
{
	if (!modelIdStr.isEmpty() && m_specifiedModelCatMap.count(modelIdStr) > 0)
	{
		return m_specifiedModelCatMap[modelIdStr];
	}

	if (m_modelCatMapSunCG.count(catName) > 0)
	{
		return m_modelCatMapSunCG[catName];
	}

	return catName;
}

void ModelDatabase::loadSpecifiedCatMap()
{
	QString catMapFileName = m_projectPath + "/meta_data/SpecifiedModelCategory.txt";

	QFile modelCatFile(catMapFileName);

	if (!modelCatFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		std::cout << "Cannot open model category map file" << catMapFileName.toStdString() << "\n";
	}

	QTextStream ifs(&modelCatFile);

	while (!ifs.atEnd())
	{
		QString currLine = ifs.readLine();
		QStringList catNames = currLine.split(",");

		// save model string if it does not exist in the map
		// catNames[0] is model id string actually
		if (m_specifiedModelCatMap.find(catNames[0]) == m_specifiedModelCatMap.end())
		{
			m_specifiedModelCatMap[catNames[0]] = catNames[1];
		}
	}

	modelCatFile.close();
}

void ModelDatabase::loadShapeNetSemTxt()
{
	std::cout << "\t Loading ShapeNet model annotation ...\n";
	std::cout << "\t \t shapeNet meta file: " << m_dbMetaFileType.toStdString() <<"\n";
	std::cout << "\t \t local model folder: " << m_dbPath.toStdString()<<"\n";

	//QString shapeNetSemTxtFileName = m_dbPath + "/" + m_dbMetaFileType + ".txt";
	QString shapeNetSemTxtFileName = m_projectPath + "/meta_data/" + m_dbMetaFileType + ".txt";

	auto lines = GetFileLines(shapeNetSemTxtFileName.toStdString(), 3);
	modelMetaInfoStrings = std::vector<std::string>(lines.begin()+1, lines.end());

	// parsing from second line
	for (int i = 1; i < lines.size(); i++)
	{
		auto parts = PartitionString(lines[i], ",", "\"");

		QString modelIdStr = QString(parts[0].c_str());
		
		modelIdStr.remove("wss.");

		DBMetaModel *candiModel = new DBMetaModel(modelIdStr);
		candiModel->dbID = i - 1;

		// right dir
		if (parts[4] == "")
		{
			candiModel->upDir = MathLib::Vector3(0, 0, 1); // default up dir
		}
		else
		{
			std::vector<int> dirElementList = StringToIntegerList(parts[4], "", "\,");
			candiModel->upDir = MathLib::Vector3(dirElementList[0], dirElementList[1], dirElementList[2]);
		}


		// front dir
		if (parts[5] == "")
		{
			candiModel->frontDir = MathLib::Vector3(0, -1, 0); // default front dir
		}
		else
		{
			std::vector<int> dirElementList = StringToIntegerList(parts[5], "", "\,");
			candiModel->frontDir = MathLib::Vector3(dirElementList[0], dirElementList[1], dirElementList[2]);
		}

		if (parts[6] != "")   // some model's scale is empty
		{
			candiModel->setScale(QString(parts[6].c_str()).toDouble());  // the 7-th entry in each line is the unit(scale)
		}
		
		dbMetaModels[modelIdStr] = candiModel;
		m_modelNum++;

		if (parts.size() >= 2 && parts[1].size() > 0)
		{
			QString catNames = QString(parts[1].c_str());

			if (catNames.contains("\""))
			{
				catNames.remove("\"");
			}

			catNames = catNames.toLower();

			// split cat names
			auto catNameList = PartitionString(catNames.toStdString(), ",");

			for (int c = 0; c < catNameList.size(); c++)
			{
				QString currCatName = QString(catNameList[c].c_str());
				currCatName = getUpdatedModelCat(currCatName, modelIdStr);

				candiModel->addCandidateCatName(currCatName); // model could have multiple category names

				if (dbCategories.count(currCatName) == 0)
				{
					Category *cat = new Category(currCatName);
					cat->setCatgoryLevel(c);
					dbCategories[currCatName] = cat;
					dbCategories[currCatName]->addInstance(candiModel);

					if (c == 0)
					{
						m_parentCatNum++;
					}
				}
				else
				{
					dbCategories[currCatName]->addInstance(candiModel);
				}
			}

			// extract attributes from shape net cat names
			candiModel->extractAttributeFromCandidateCatNames();

			// set sub-category
			for (int c = 1; c < catNameList.size(); c++)
			{
				dbCategories[catNameList[0].c_str()]->addSubCatNames(QString(catNameList[c].c_str()));
			}
		}

		// read wnlemmas
		if (parts.size() >= 4 && parts[3].size() > 0)
		{
			QString wnLemmas = QString(parts[3].c_str());

			if (wnLemmas.contains("\""))
			{
				wnLemmas.remove("\"");
			}

			wnLemmas = wnLemmas.toLower();

			// split cat names
			auto wnLemmaList = PartitionString(wnLemmas.toStdString(), ",");
			for (int w = 0; w < wnLemmaList.size(); w++)
			{
				candiModel->addWordNetLemmas(QString(wnLemmaList[w].c_str()));
			}
		}
	}

	std::cout << "Model annotation loading done.\n";
}

DBMetaModel* ModelDatabase::getMetaModelByNameString(const QString &s)
{
	return dbMetaModels[s]; 
}

void ModelDatabase::loadSunCGMetaData()
{
	std::cout << "\t Loading SunCG meta data ...\n";

	QString sunCGMetaDataFileName = m_projectPath + "/meta_data/modelsSunCG.csv";

	auto lines = GetFileLines(sunCGMetaDataFileName.toStdString(), 3);
	modelMetaInfoStrings = std::vector<std::string>(lines.begin() + 1, lines.end());

	// parsing from second line
	for (int i = 1; i < lines.size(); i++)
	{
		auto parts = PartitionString(lines[i], ",", "\"");

		QString modelIdStr = QString(parts[0].c_str());

		DBMetaModel *candiModel = new DBMetaModel(modelIdStr);
		candiModel->dbID = i - 1;

		// front dir
		if (parts[1] == "")
		{
			candiModel->frontDir = MathLib::Vector3(0, 0, 1); // default front dir
		}
		else
		{
			std::vector<int> dirElementList = StringToIntegerList(parts[1], "", "\,");
			candiModel->frontDir = MathLib::Vector3(dirElementList[0], dirElementList[1], dirElementList[2]);
		}

		dbMetaModels[modelIdStr] = candiModel;
		m_modelNum++;
	}

	std::cout << "Model annotation loading done.\n";
}

void ModelDatabase::loadSunCGModelCatMap()
{
	QString sunCGCatMapFileName = m_projectPath + "/meta_data/ModelCategoryMapSunCG.txt";

	QFile modelCatFile(sunCGCatMapFileName);

	if (!modelCatFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		std::cout << "Cannot open model category map file "<< sunCGCatMapFileName.toStdString() <<"\n";
	}

	QTextStream ifs(&modelCatFile);

	while (!ifs.atEnd())
	{
		QString currLine = ifs.readLine();
		QStringList catNames = currLine.split(",");

		// save category if it does not exist in the map
		if (m_modelCatMapSunCG.find(catNames[0]) == m_modelCatMapSunCG.end())
		{
			m_modelCatMapSunCG[catNames[0]] = catNames[1];
		}
	}

	modelCatFile.close();
}

void ModelDatabase::loadSunCGModelCat()
{
	std::cout << "\t Loading SunCG model category...\n";

	QString sunCGMetaDataFileName = m_projectPath + "/meta_data/ModelCategoryAnnoSunCG.csv";

	auto lines = GetFileLines(sunCGMetaDataFileName.toStdString(), 3);
	modelMetaInfoStrings = std::vector<std::string>(lines.begin() + 1, lines.end());

	// parsing from second line
	for (int i = 1; i < lines.size(); i++)
	{
		auto parts = PartitionString(lines[i], ",", "\"");

		QString modelIdStr = QString(parts[1].c_str());

		if (!isModelInDB(modelIdStr)) continue;

		DBMetaModel *candiModel = dbMetaModels[modelIdStr];

		// category
		QStringList catNameList;
		QString cateName = QString(parts[3].c_str());  

		cateName = getUpdatedModelCat(cateName, modelIdStr);
		catNameList.push_back(cateName);   // use the coarse category name as the parent/base category name



		cateName = getUpdatedModelCat(QString(parts[2].c_str()), modelIdStr);
		catNameList.push_back(cateName);  // the fine category is the sub-category
		candiModel->setCatName(cateName); // use the fine category as the model category; also, fine category are already mapped to stanford catgories
		
		for (int c = 0; c < catNameList.size(); c++)
		{
			QString currCatName = QString(catNameList[c]);
			candiModel->addCandidateCatName(currCatName); // model could have multiple category names

			if (dbCategories.count(currCatName) == 0)
			{
				Category *cat = new Category(currCatName);
				cat->setCatgoryLevel(c);
				dbCategories[currCatName] = cat;
				dbCategories[currCatName]->addInstance(candiModel);

				if (c == 0)
				{
					m_parentCatNum++;
				}
			}
			else
			{
				dbCategories[currCatName]->addInstance(candiModel);
			}
		}

		// extract attributes from shape net cat names
		candiModel->extractAttributeFromCandidateCatNames();

		// set sub-category
		for (int c = 1; c < catNameList.size(); c++)
		{
			dbCategories[catNameList[0]]->addSubCatNames(QString(catNameList[c]));
		}
	}


	// save category names if not done before
	QString modelCatFileName = m_projectPath + "/meta_data/suncg_model_category.txt";

	QFile outFile(modelCatFileName);
	QTextStream ofs(&outFile);

	if (!outFile.exists())
	{
		if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
		{
			std::cout << "\tCannot open file " << modelCatFileName.toStdString() << std::endl;
		}

		for (auto it = dbCategories.begin(); it != dbCategories.end(); it++)
		{
			QString catName = it->first;
			ofs << catName << "\n";
		}
	}

	std::cout << "Model annotation loading done.\n";
}

bool ModelDatabase::isModelInDB(const QString &s)
{
	if (dbMetaModels.find(s)!= dbMetaModels.end())
	{
		return true;
	}

	else
	{
		return false;
	}
}

DBMetaModel::DBMetaModel()
{
	m_idStr = "";
	m_categoryName = "";
	m_scale = 1.0;
	m_initTrans = MathLib::Matrix4d::Identity_Matrix;

	dbID = -1;
	frontDir = MathLib::Vector3(0, -1, 0);
	upDir = MathLib::Vector3(0, 0, 1);
	position = MathLib::Vector3(0, 0, 0);
	
	parentId = -1;
	onSuppPlaneUV = std::vector<double>(2,0.5);
	positionToSuppPlaneDist = 0; // position to support plane dist after the transformation to real world unit

	m_isCatNameProcessed = false;
}

DBMetaModel::DBMetaModel(const QString &s)
{
	m_idStr = s;
	m_categoryName = "";
	m_scale = 1.0;
	m_initTrans = MathLib::Matrix4d::Identity_Matrix;

	dbID = -1;
	frontDir = MathLib::Vector3(0, -1, 0);
	upDir = MathLib::Vector3(0, 0, 1);
	position = MathLib::Vector3(0, 0, 0);

	parentId = -1;
	onSuppPlaneUV = std::vector<double>(2, 0.5);
	positionToSuppPlaneDist = 0; // position to support plane dist after the transformation to real world unit

	m_isCatNameProcessed = false;
}

// copy from m
DBMetaModel::DBMetaModel(DBMetaModel *m)
{
	m_idStr = m->m_idStr;
	m_categoryName = m->m_categoryName;
	m_scale = m->m_scale;
	m_initTrans = m->m_initTrans;

	m_attributes = m->m_attributes;

	dbID = m->dbID;
	frontDir = m->frontDir;
	upDir = m->upDir;
	position = m->position;
}

QString DBMetaModel::getProcessedCatName()
{
	if (m_isCatNameProcessed)
	{
		return m_processedCatName;
	}

	//if (m_idStr == "416674f64be11975bc4f8438441dcb1d")
	//{
	//	m_isCatNameProcessed = true;
	//	m_processedCatName = "monitor";
	//	return m_processedCatName;
	//}

	m_processedCatName = "";

	if (m_CandidateCategoryNames.empty() && m_wordNetLemmas.empty())
	{
		m_isCatNameProcessed = true;
		return m_processedCatName;
	}

	const int badCatNum = 12;
	QString badCatNames[badCatNum] = { "_stanfordscenedbmodels", "_scenegallerymodels", "_oimwhitelist", "_attributestrain", 
		"_attributes", "_evalsetinscenes", "_pilotstudymodels", "_geoautotagevalset", "_randomsetstudymodels", "_evalsetexclude","drinkingutensil", "fooditem"};

	for (int i = 0; i < badCatNum; i++)
	{
		auto it = std::find(m_CandidateCategoryNames.begin(), m_CandidateCategoryNames.end(), badCatNames[i]);
		if (it != m_CandidateCategoryNames.end())
			m_CandidateCategoryNames.erase(it);
	}

	if (m_CandidateCategoryNames.empty())
	{
		if (!m_wordNetLemmas.empty())
		{
			m_processedCatName = m_wordNetLemmas[0];
		}
		else
		{
			m_processedCatName = "noname";
		}
		
		m_isCatNameProcessed = true;
		return m_processedCatName;
	}

	m_processedCatName = m_CandidateCategoryNames[0];

	if (m_CandidateCategoryNames[0] == "chestofdrawers")
	{
		if (m_CandidateCategoryNames.size()>1)
		{
			m_processedCatName = m_CandidateCategoryNames[1];
		}
	}

	
	if (m_CandidateCategoryNames[0] == "lamp")
	{
		for (int i = 1; i < m_CandidateCategoryNames.size(); i++)
		{
			if (m_CandidateCategoryNames[i] == "desklamp")
			{
				m_processedCatName = "desklamp";
				break;
			}

			if (m_CandidateCategoryNames[i] == "floorlamp")
			{
				m_processedCatName = "floorlamp";
				break;
			}
		}
	}

	// book
	if (m_CandidateCategoryNames[0] == "book")
	{
		for (int i = 1; i < m_CandidateCategoryNames.size(); i++)
		{
			if (m_CandidateCategoryNames[i] == "openbook")
			{
				m_processedCatName = "openbook";
				break;
			}

			if (m_CandidateCategoryNames[i] == "standbook")
			{
				m_processedCatName = "standbook";
				break;
			}
		}
	}

	// books
	if (m_CandidateCategoryNames[0] == "books")
	{
		for (int i = 1; i < m_CandidateCategoryNames.size(); i++)
		{
			if (m_CandidateCategoryNames[i] == "standbooks")
			{
				m_processedCatName = "standbooks";
				break;
			}

			if (m_CandidateCategoryNames[i] == "stackbooks")
			{
				m_processedCatName = "stackbooks";
				break;
			}
		}
	}

	if (m_CandidateCategoryNames[0] == "computer")
	{
		for (int i = 1; i < m_CandidateCategoryNames.size(); i++)
		{
			if (m_CandidateCategoryNames[i] == "laptop")
			{
				m_processedCatName = "laptop";
				break;
			}
		}
	}

	m_isCatNameProcessed = true;
	return m_processedCatName;
}

const QString& DBMetaModel::getShapeNetCatsStr()
{
	return QString();
}

DBMetaModel::~DBMetaModel()
{

}

void DBMetaModel::extractAttributeFromCandidateCatNames()
{
	for (int i = 0; i < m_CandidateCategoryNames.size(); i++)
	{
		if (m_CandidateCategoryNames[i].contains("office"))
		{
			m_attributes.push_back("office");
		}

		if (m_CandidateCategoryNames[i].contains("coffee"))
		{
			m_attributes.push_back("coffee");
		}

		//if (m_shapeNetCategoryNames[i].contains("file"))
		//{
		//	m_attributes.push_back("file");
		//}

		if (m_CandidateCategoryNames[i].contains("queen"))
		{
			m_attributes.push_back("queen");
		}

		if (m_CandidateCategoryNames[i].contains("dining"))
		{
			m_attributes.push_back("dining");
		}

		if (m_CandidateCategoryNames[i].contains("round"))
		{
			m_attributes.push_back("round");
		}

		if (m_CandidateCategoryNames[i].contains("sauce"))
		{
			m_attributes.push_back("sauce");
			m_CandidateCategoryNames[i] = "bottle";
		}

		if (m_CandidateCategoryNames[i].contains("recliner") || m_CandidateCategoryNames[i].contains("accentchair")
			|| m_CandidateCategoryNames[i].contains("beanbag")) // sofa chair
		{
			m_attributes.push_back("sofa");  
		}
	}

	// for table, if it is not round, then it is rectangular
	for (int i = 0; i < m_CandidateCategoryNames.size(); i++)
	{
		if (m_CandidateCategoryNames[i].contains("table")
			&& std::find(m_attributes.begin(), m_attributes.end(), QString("dining")) != m_attributes.end()
			&& std::find(m_attributes.begin(), m_attributes.end(), QString("round")) == m_attributes.end()
			&& std::find(m_attributes.begin(), m_attributes.end(), QString("rectangular")) == m_attributes.end())
		{
			m_attributes.push_back("rectangular");
			break;
		}
	}
}
