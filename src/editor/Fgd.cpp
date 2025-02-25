#include "lang.h"
#include "Fgd.h"
#include "log.h"

#include <regex>

std::map<std::string, int> fgdKeyTypes{
	{"integer", FGD_KEY_INTEGER},
	{"choices", FGD_KEY_CHOICES},
	{"flags", FGD_KEY_FLAGS},
	{"color255", FGD_KEY_RGB},
	{"studio", FGD_KEY_STUDIO},
	{"sound", FGD_KEY_SOUND},
	{"sprite", FGD_KEY_SPRITE},
	{"target_source", FGD_KEY_TARGET_SRC},
	{"target_destination", FGD_KEY_TARGET_DST}
};


Fgd::~Fgd()
{
	for (size_t i = 0; i < classes.size(); i++)
	{
		delete classes[i];
	}
}

FgdClass* Fgd::getFgdClass(const std::string& cname)
{
	auto it = std::find_if(classes.begin(), classes.end(), [&cname](const auto& fgdClass) {
		return fgdClass->name == cname;
		});

	return (it != classes.end()) ? *it : NULL;
}

FgdClass* Fgd::getFgdClass(const std::string& cname, int type)
{
	auto it = std::find_if(classes.begin(), classes.end(), [&cname, &type](const auto& fgdClass) {
		return fgdClass->name == cname && fgdClass->classType == type;
		});

	return (it != classes.end()) ? *it : NULL;
}

void Fgd::merge(Fgd* other)
{
	if (path.empty() && other->path.size())
	{
		this->path = other->path;
		this->name = stripExt(basename(path));
		this->lineNum = 0;
	}

	for (FgdClass* otherClass : other->classes)
	{
		auto it = std::find_if(classes.begin(), classes.end(), [&otherClass](const auto& fgdClass) {
			return fgdClass->name == otherClass->name && fgdClass->classType == otherClass->classType;
			});

		if (it != classes.end())
		{
			// Here keyvalues can be merged

			print_log(get_localized_string(LANG_0299), otherClass->name, other->name);
			continue;
		}
		else
		{
			classes.push_back(new FgdClass(*otherClass));
		}
	}

	processClassInheritance();
	createEntGroups();
	setSpawnflagNames();
}

bool Fgd::parse()
{
	if (!fileExists(path))
	{
		return false;
	}

	std::regex brackEnd(R"(\s*\[\s*\]\s*$)");


	print_log(get_localized_string(LANG_0300), path);

	std::ifstream in(path);

	lineNum = 0;
	line.clear();

	std::vector<std::string> inputLines;
	std::vector<int> inputLineNums;
	while (std::getline(in, line))
	{
		lineNum++;
		line = trimSpaces(line);

		if (line.empty() || starts_with(line,"//"))
			continue;

		if (line[0] == '[' || line[0] == ']')
		{
			inputLines.push_back(line);
			inputLineNums.push_back(lineNum);
		}
		else if (ends_with(line,'['))
		{
			line.pop_back();
			inputLines.push_back(line);
			inputLines.push_back("[");
			inputLineNums.push_back(lineNum);
			inputLineNums.push_back(lineNum);
		}
		else
		{
			replaceAll(line, "//", " :: :: ");
			if (line.find_first_of('[') != std::string::npos)
			{
				auto split = splitStringIgnoringQuotes(line, "[");
				if (split.size())
				{
					bool added = false;
					for (auto& s : split)
					{
						s = trimSpaces(s);

						if (s.empty())
							continue;

						inputLines.push_back(s);
						inputLines.push_back("[");
						inputLineNums.push_back(lineNum);
						inputLineNums.push_back(lineNum);
						added = true;
					}
					if (added)
					{
						inputLineNums.pop_back();
						inputLines.pop_back();
					}
				}
			}
			else if (line.find_first_of(']') != std::string::npos)
			{
				auto split = splitStringIgnoringQuotes(line, "]");
				if (split.size())
				{
					bool added = false;
					for (auto& s : split)
					{
						s = trimSpaces(s);

						if (s.empty())
							continue;

						inputLines.push_back(s);
						inputLines.push_back("]");
						inputLineNums.push_back(lineNum);
						inputLineNums.push_back(lineNum);
						added = true;
					}
					if (added)
					{
						inputLineNums.pop_back();
						inputLines.pop_back();
					}
				}
			}
			else if (ends_with(line,']'))
			{
				line.pop_back();
				inputLines.push_back(line);
				inputLines.push_back("\n");
				inputLines.push_back("]");
				inputLineNums.push_back(lineNum);
				inputLineNums.push_back(lineNum);
				inputLineNums.push_back(lineNum);
			}
			else
			{
				inputLines.push_back(line);
				inputLineNums.push_back(lineNum);
			}
		}
	}

	//std::ostringstream outs;
	//for (const auto& s : inputLines)
	//{
	//	outs << s << "\n";
	//}
	//writeFile(path + "_test.fgd", outs.str());

	FgdClass* fgdClass = new FgdClass();
	int bracketNestLevel = 0;

	line.clear();
	for (size_t i = 0; i < inputLines.size(); i++)
	{
		line = inputLines[i];
		lineNum = inputLineNums[i];

		if (line[0] == '@')
		{
			if (bracketNestLevel)
			{
				print_log(get_localized_string(LANG_0301), lineNum, name);

				if (fgdClass->isSprite && !fgdClass->sprite.size())
				{
					for (auto& kv : fgdClass->keyvalues)
					{
						if (kv.name == "model" && kv.defaultValue.size())
						{
							fgdClass->sprite = kv.defaultValue;
							fixupPath(fgdClass->sprite, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
							break;
						}
					}
				}

				classes.push_back(fgdClass);
				fgdClass = new FgdClass();
				bracketNestLevel = 0;
			}

			parseClassHeader(*fgdClass);
		}

		if ((line.size() && line[0] == '['))
		{
			bracketNestLevel++;
		}

		if (line.size() && (line[0] == ']' || line[line.size() - 1] == ']'))
		{
			bracketNestLevel--;
			if (bracketNestLevel == 0)
			{
				if (fgdClass->isSprite && !fgdClass->sprite.size())
				{
					for (auto& kv : fgdClass->keyvalues)
					{
						if (kv.name == "model" && kv.defaultValue.size())
						{
							fgdClass->sprite = kv.defaultValue;
							fixupPath(fgdClass->sprite, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
							break;
						}
					}
				}
				classes.push_back(fgdClass);
				fgdClass = new FgdClass();
			}
		}

		if (bracketNestLevel == 0 && (line.rfind('[') != std::string::npos && std::regex_search(line, brackEnd)))
		{
			if (fgdClass->isSprite && !fgdClass->sprite.size())
			{
				for (auto& kv : fgdClass->keyvalues)
				{
					if (kv.name == "model" && kv.defaultValue.size())
					{
						fgdClass->sprite = kv.defaultValue;
						fixupPath(fgdClass->sprite, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
						break;
					}
				}
			}
			classes.push_back(fgdClass);
			fgdClass = new FgdClass(); //memory leak
			continue;
		}

		if (line.size() && (line[0] == '[' || line[0] == ']' || line[line.size() - 1] == ']'))
		{
			continue;
		}

		if (bracketNestLevel == 1)
		{
			parseKeyvalue(*fgdClass);
		}

		if (bracketNestLevel == 2)
		{
			if (fgdClass->keyvalues.empty())
			{
				print_log(get_localized_string(LANG_0302), lineNum, name);
				continue;
			}
			KeyvalueDef& lastKey = fgdClass->keyvalues[fgdClass->keyvalues.size() - 1];
			parseChoicesOrFlags(lastKey);
		}
	}

	delete fgdClass;

	processClassInheritance();
	createEntGroups();
	setSpawnflagNames();
	return true;
}

void Fgd::parseClassHeader(FgdClass& fgdClass)
{
	std::vector<std::string> headerParts = splitString(line, "=", 2);

	if (headerParts.empty())
	{
		print_log(get_localized_string(LANG_0303), lineNum, name);
		return;
	}

	// group parts enclosed in parens or quotes
	std::vector<std::string> typeParts = splitString(trimSpaces(headerParts[0]), " ");
	typeParts = groupParts(typeParts);

	std::string classType = toLowerCase(typeParts[0]);

	if (classType == "@baseclass")
	{
		fgdClass.classType = FGD_CLASS_BASE;
	}
	else if (classType == "@solidclass")
	{
		fgdClass.classType = FGD_CLASS_SOLID;
	}
	else if (classType == "@pointclass")
	{
		fgdClass.classType = FGD_CLASS_POINT;
	}
	else
	{
		print_log(get_localized_string(LANG_0304), typeParts[0], name);
	}

	// parse constructors/properties
	for (size_t i = 1; i < typeParts.size(); i++)
	{
		std::string lpart = toLowerCase(typeParts[i]);

		if (starts_with(lpart,"base("))
		{
			std::vector<std::string> baseClassList = splitString(getValueInParens(typeParts[i]), ",");
			for (size_t k = 0; k < baseClassList.size(); k++)
			{
				std::string baseClass = trimSpaces(baseClassList[k]);
				fgdClass.baseClasses.push_back(baseClass);
			}
		}
		else if (starts_with(lpart,"size("))
		{
			std::vector<std::string> sizeList = splitString(getValueInParens(typeParts[i]), ",");

			if (sizeList.size() == 1)
			{
				vec3 size = parseVector(sizeList[0]);
				fgdClass.mins = size * -0.5f;
				fgdClass.maxs = size * 0.5f;
			}
			else if (sizeList.size() == 2)
			{
				fgdClass.mins = parseVector(sizeList[0]);
				fgdClass.maxs = parseVector(sizeList[1]);
			}
			else
			{
				print_log(get_localized_string(LANG_0305), lineNum, name);
			}

			fgdClass.sizeSet = true;
		}
		else if (starts_with(lpart,"color("))
		{
			std::vector<std::string> nums = splitString(getValueInParens(typeParts[i]), " ");

			if (nums.size() == 3)
			{
				fgdClass.color = { (unsigned char)str_to_int(nums[0]), (unsigned char)str_to_int(nums[1]), (unsigned char)str_to_int(nums[2]) };
			}
			else
			{
				print_log(get_localized_string(LANG_0306), lineNum, name);
			}
			fgdClass.colorSet = true;
		}
		else if (starts_with(lpart,"offset("))
		{
			std::vector<std::string> nums = splitString(getValueInParens(typeParts[i]), " ");

			if (nums.size() == 3)
			{
				fgdClass.offset = { str_to_float(nums[0]), str_to_float(nums[1]),str_to_float(nums[2]) };
			}
			else
			{
				print_log(get_localized_string("LANG_FGD_BAD_OFFSET"), lineNum, name);
			}
		}
		else if (starts_with(lpart,"studio("))
		{
			std::string mdlpath = getValueInParens(typeParts[i]);
			if (mdlpath.size())
			{
				fgdClass.model = std::move(mdlpath);
				fixupPath(fgdClass.model, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
			}
			fgdClass.isModel = true;
		}
		else if (starts_with(lpart,"sequence("))
		{
			fgdClass.modelSequence = str_to_int(getValueInParens(typeParts[i]));
		}
		else if (starts_with(lpart,"body("))
		{
			fgdClass.modelBody = str_to_int(getValueInParens(typeParts[i]));
		}
		else if (starts_with(lpart,"iconsprite("))
		{
			fgdClass.sprite = getValueInParens(typeParts[i]);
			fgdClass.isSprite = true;
			if (fgdClass.sprite.size())
				fixupPath(fgdClass.sprite, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
		}
		else if (starts_with(lpart,"sprite("))
		{
			fgdClass.sprite = getValueInParens(typeParts[i]);
			fgdClass.isSprite = true;
			if (fgdClass.sprite.size())
				fixupPath(fgdClass.sprite, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
		}
		else if (starts_with(lpart,"decal("))
		{
			fgdClass.isDecal = true;
		}
		else if (starts_with(lpart,"flags("))
		{
			std::vector<std::string> flagsList = splitString(getValueInParens(typeParts[i]), ",");
			for (size_t k = 0; k < flagsList.size(); k++)
			{
				std::string flag = trimSpaces(flagsList[k]);
				if (flag == "Angle")
					fgdClass.hasAngles = true; // force using angles/angle key ?
				else if (flag == "Path" || flag == "Light")
					;
				else
					print_log(get_localized_string(LANG_0307), flag, lineNum, name);
			}
		}
		else if (typeParts[i].find('(') != std::string::npos)
		{
			std::string typeName = typeParts[i].substr(0, typeParts[i].find('('));
			print_log(get_localized_string(LANG_0308), typeName, lineNum, name);
		}
	}

	if (headerParts.size() <= 1)
	{
		print_log(get_localized_string(LANG_1048), lineNum, name);
		return;
	}

	std::vector<std::string> nameParts = splitStringIgnoringQuotes(headerParts[1], ":");
	if (nameParts.size() >= 1)
	{
		fgdClass.name = trimSpaces(nameParts[0]);
		// strips brackets if they're there
		// fgdClass.name = fgdClass.name.substr(0, fgdClass.name.find(' '));
		nameParts.erase(nameParts.begin());
	}

	fgdClass.description = "";
	if (nameParts.size() >= 1)
	{
		for (size_t i = 0; i < nameParts.size(); i++)
		{
			std::string input = getValueInQuotes(nameParts[i]);
			trimSpaces(input);
			if (input.size())
				fgdClass.description += input + "\n";
		}

		if (fgdClass.description.size())
			fgdClass.description.pop_back();
	}
}

void Fgd::parseKeyvalue(FgdClass& outClass)
{
	std::vector<std::string> keyParts = splitStringIgnoringQuotes(line, ":");

	KeyvalueDef def;

	def.name = keyParts[0].substr(0, keyParts[0].find('('));
	def.valueType = toLowerCase(getValueInParens(keyParts[0]));

	def.iType = FGD_KEY_STRING;
	if (fgdKeyTypes.find(def.valueType) != fgdKeyTypes.end())
	{
		def.iType = fgdKeyTypes[def.valueType];
	}

	if (keyParts.size() > 1)
		def.shortDescription = getValueInQuotes(keyParts[1]);
	else
	{
		def.shortDescription = def.name;

		// capitalize (infodecal)
		if ((def.shortDescription[0] > 96) && (def.shortDescription[0] < 123))
			def.shortDescription[0] = def.shortDescription[0] - 32;
	}

	if (keyParts.size() > 2)
	{
		if (keyParts[2].find('=') != std::string::npos)
		{ // choice
			def.defaultValue = trimSpaces(keyParts[2].substr(0, keyParts[2].find('=')));
		}
		else if (keyParts[2].find('\"') != std::string::npos)
		{ // std::string
			def.defaultValue = getValueInQuotes(keyParts[2]);
		}
		else
		{ // integer
			def.defaultValue = trimSpaces(keyParts[2]);
		}
		def.fullDescription = "";
		if (keyParts.size() > 3)
		{
			for (size_t i = 3; i < keyParts.size(); i++)
			{
				std::string input = getValueInQuotes(keyParts[i]);
				trimSpaces(input);
				if (input.size())
					def.fullDescription += input + "\n";
			}

			if (def.fullDescription.size())
				def.fullDescription.pop_back();
		}
	}

	outClass.keyvalues.push_back(def);

	//print_log << "ADD KEY " << def.name << "(" << def.valueType << ") : " << def.description << " : " << def.defaultValue << endl;
}

void Fgd::parseChoicesOrFlags(KeyvalueDef& outKey)
{
	std::vector<std::string> keyParts = splitStringIgnoringQuotes(line, ":");

	KeyvalueChoice def;

	if (keyParts[0].find('\"') != std::string::npos)
	{
		def.svalue = getValueInQuotes(keyParts[0]);
		def.ivalue = 0;
		def.isInteger = false;
	}
	else
	{
		def.svalue = trimSpaces(keyParts[0]);
		def.ivalue = str_to_int(keyParts[0]);
		def.isInteger = true;
	}

	if (keyParts.size() > 1)
		def.name = getValueInQuotes(keyParts[1]);

	if (keyParts.size() > 2)
		def.sdefvalue = keyParts[2];

	def.fullDescription = "";

	if (keyParts.size() > 3)
	{
		for (size_t i = 3; i < keyParts.size(); i++)
		{
			std::string input = getValueInQuotes(keyParts[i]);
			trimSpaces(input);
			if (input.size())
				def.fullDescription += input + "\n";
		}
		if (def.fullDescription.size())
			def.fullDescription.pop_back();
	}

	outKey.choices.push_back(def);
}

void Fgd::processClassInheritance()
{
	for (size_t i = 0; i < classes.size(); i++)
	{
		if (classes[i]->classType == FGD_CLASS_BASE)
			continue;

		std::vector<FgdClass*> allBaseClasses;
		classes[i]->getBaseClasses(this, allBaseClasses);

		if (!allBaseClasses.empty())
		{
			std::vector<KeyvalueDef> newKeyvalues;
			std::vector<KeyvalueChoice> newSpawnflags;
			std::set<std::string> addedKeys;
			std::set<std::string> addedSpawnflags;
			//print_log << classes[i]->name << " INHERITS FROM: ";
			for (int k = (int)allBaseClasses.size() - 1; k >= 0; k--)
			{
				if (!classes[i]->colorSet && allBaseClasses[k]->colorSet)
				{
					classes[i]->color = allBaseClasses[k]->color;
				}
				if (!classes[i]->sizeSet && allBaseClasses[k]->sizeSet)
				{
					classes[i]->mins = allBaseClasses[k]->mins;
					classes[i]->maxs = allBaseClasses[k]->maxs;
				}
				auto tmpBaseClass = allBaseClasses[k];
				for (size_t c = 0; c < tmpBaseClass->keyvalues.size(); c++)
				{
					auto & tmpBaseKeys = tmpBaseClass->keyvalues[c];
					if (addedKeys.find(tmpBaseKeys.name) == addedKeys.end())
					{
						newKeyvalues.push_back(tmpBaseClass->keyvalues[c]);
						addedKeys.insert(tmpBaseKeys.name);
					}
					if (tmpBaseKeys.iType == FGD_KEY_FLAGS)
					{
						for (size_t f = 0; f < tmpBaseKeys.choices.size(); f++)
						{
							KeyvalueChoice& spawnflagOption = tmpBaseKeys.choices[f];
							if (addedSpawnflags.find(spawnflagOption.svalue) == addedSpawnflags.end())
							{
								newSpawnflags.push_back(spawnflagOption);
								addedSpawnflags.insert(spawnflagOption.svalue);
							}
						}
					}
				}
				//print_log << allBaseClasses[k]->name << " ";
			}

			for (size_t c = 0; c < classes[i]->keyvalues.size(); c++)
			{
				auto tmpBaseKeys = classes[i]->keyvalues[c];
				if (addedKeys.find(tmpBaseKeys.name) == addedKeys.end())
				{
					newKeyvalues.push_back(tmpBaseKeys);
					addedKeys.insert(tmpBaseKeys.name);
				}
				if (tmpBaseKeys.iType == FGD_KEY_FLAGS)
				{
					for (size_t f = 0; f < tmpBaseKeys.choices.size(); f++)
					{
						KeyvalueChoice& spawnflagOption = tmpBaseKeys.choices[f];
						if (addedSpawnflags.find(spawnflagOption.svalue) == addedSpawnflags.end())
						{
							newSpawnflags.push_back(spawnflagOption);
							addedSpawnflags.insert(spawnflagOption.svalue);
						}
					}
				}
			}

			std::vector<KeyvalueChoice> oldchoices;
			for (size_t c = 0; c < classes[i]->keyvalues.size(); c++)
			{
				if (classes[i]->keyvalues[c].iType == FGD_KEY_FLAGS)
				{
					oldchoices = classes[i]->keyvalues[c].choices;
				}
			}


			classes[i]->keyvalues = std::move(newKeyvalues);

			for (size_t c = 0; c < classes[i]->keyvalues.size(); c++)
			{
				if (classes[i]->keyvalues[c].iType == FGD_KEY_FLAGS)
				{
					classes[i]->keyvalues[c].choices = newSpawnflags;

					for (auto& choise : classes[i]->keyvalues[c].choices)
					{
						for (auto choiseOld : oldchoices)
						{
							if (choise.ivalue == choiseOld.ivalue)
							{
								choise = choiseOld;
							}
						}
					}
				}
			}

			for (size_t c = 0; c < classes[i]->keyvalues.size(); c++)
			{
				if (classes[i]->keyvalues[c].iType == FGD_KEY_STUDIO)
				{
					if (classes[i]->keyvalues[c].name == "model")
					{
						if (!classes[i]->model.size())
						{
							classes[i]->model = classes[i]->keyvalues[c].defaultValue;
							fixupPath(classes[i]->model, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
						}
					}
				}
				if (classes[i]->keyvalues[c].iType == FGD_KEY_CHOICES)
				{
					if (classes[i]->keyvalues[c].name == "model")
					{
						if (!classes[i]->model.size())
						{
							classes[i]->model = classes[i]->keyvalues[c].defaultValue;
							fixupPath(classes[i]->model, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE, FIXUPPATH_SLASH::FIXUPPATH_SLASH_REMOVE);
						}
					}
					else if (classes[i]->keyvalues[c].name == "sequence")
					{
						if (classes[i]->modelSequence <= 0)
						{
							if (classes[i]->keyvalues[c].iType == FGD_KEY_TYPES::FGD_KEY_INTEGER)
							{
								classes[i]->modelSequence = str_to_int(classes[i]->keyvalues[c].defaultValue);
							}
						}
					}
					else if (classes[i]->keyvalues[c].name == "body")
					{
						if (classes[i]->modelBody <= 0)
						{
							if (classes[i]->keyvalues[c].iType == FGD_KEY_TYPES::FGD_KEY_INTEGER)
							{
								classes[i]->modelBody = str_to_int(classes[i]->keyvalues[c].defaultValue);
							}
						}
					}
					else if (classes[i]->keyvalues[c].name == "skin")
					{
						if (classes[i]->modelSkin <= 0)
						{
							if (classes[i]->keyvalues[c].iType == FGD_KEY_TYPES::FGD_KEY_INTEGER)
							{
								classes[i]->modelSkin = str_to_int(classes[i]->keyvalues[c].defaultValue);
							}
						}
					}
					else if (classes[i]->keyvalues[c].name == "scale" || 
						classes[i]->keyvalues[c].name == "sprite_scale")
					{
						if (classes[i]->keyvalues[c].iType == FGD_KEY_TYPES::FGD_KEY_INTEGER)
						{
							classes[i]->scale = str_to_int(classes[i]->keyvalues[c].defaultValue) * 1.0f;
						}
						else if (classes[i]->keyvalues[c].iType == FGD_KEY_TYPES::FGD_KEY_STRING)
						{
							classes[i]->scale = str_to_float(classes[i]->keyvalues[c].defaultValue);
						}
					}
				}
			}
		}
	}
}

void FgdClass::getBaseClasses(Fgd* fgd, std::vector<FgdClass*>& inheritanceList)
{
	if (!baseClasses.empty())
	{
		for (int i = (int)baseClasses.size() - 1; i >= 0; i--)
		{
			auto it = std::find_if(fgd->classes.begin(), fgd->classes.end(), [this, i](const auto& fgdClass) {
				return fgdClass->name == baseClasses[i];
				});

			if (it == fgd->classes.end())
			{
				print_log(get_localized_string(LANG_0310), baseClasses[i], name);
				continue;
			}

			inheritanceList.push_back(*it);
			(*it)->getBaseClasses(fgd, inheritanceList);
		}
	}
}

void Fgd::createEntGroups()
{
	solidEntGroups.clear();
	pointEntGroups.clear();

	std::set<std::string> addedPointGroups;
	std::set<std::string> addedSolidGroups;

	for (size_t i = 0; i < classes.size(); i++)
	{
		if (classes[i]->classType == FGD_CLASS_BASE || classes[i]->name == "worldspawn")
			continue;
		std::string cname = classes[i]->name;
		std::string groupName = cname;

		if (cname.find('_') != std::string::npos)
		{
			groupName = cname.substr(0, cname.find('_'));
		}

		bool isPointEnt = classes[i]->classType == FGD_CLASS_POINT;

		std::set<std::string>& targetSet = isPointEnt ? addedPointGroups : addedSolidGroups;
		std::vector<FgdGroup>& targetGroup = isPointEnt ? pointEntGroups : solidEntGroups;

		if (targetSet.find(groupName) == targetSet.end())
		{
			FgdGroup newGroup;
			newGroup.groupName = groupName;

			targetGroup.push_back(newGroup);
			targetSet.insert(groupName);
		}

		bool added = false;
		for (size_t k = 0; k < targetGroup.size(); k++)
		{
			if (targetGroup[k].groupName == groupName)
			{
				added = true;
				targetGroup[k].classes.push_back(classes[i]);
				break;
			}
		}
		if (!added && targetGroup.size())
		{
			targetGroup[0].classes.push_back(classes[i]);
		}
	}

	FgdGroup otherPointEnts;
	otherPointEnts.groupName = "other";
	for (size_t i = 0; i < pointEntGroups.size(); i++)
	{
		if (pointEntGroups[i].classes.size() <= 1)
		{
			otherPointEnts.classes.push_back(pointEntGroups[i].classes[0]);
			pointEntGroups.erase(pointEntGroups.begin() + i);
			i--;
		}
	}
	pointEntGroups.push_back(otherPointEnts);

	FgdGroup otherSolidEnts;
	otherSolidEnts.groupName = "other";
	for (size_t i = 0; i < solidEntGroups.size(); i++)
	{
		if (solidEntGroups[i].classes.size() <= 1)
		{
			otherSolidEnts.classes.push_back(solidEntGroups[i].classes[0]);
			solidEntGroups.erase(solidEntGroups.begin() + i);
			i--;
		}
	}
	solidEntGroups.push_back(otherSolidEnts);
}

void Fgd::setSpawnflagNames()
{
	for (size_t i = 0; i < classes.size(); i++)
	{
		if (classes[i]->classType == FGD_CLASS_BASE)
			continue;

		for (size_t k = 0; k < classes[i]->keyvalues.size(); k++)
		{
			if (classes[i]->keyvalues[k].name == "spawnflags")
			{
				for (size_t c = 0; c < classes[i]->keyvalues[k].choices.size(); c++)
				{
					KeyvalueChoice& choice = classes[i]->keyvalues[k].choices[c];

					if (!choice.isInteger)
					{
						print_log(get_localized_string(LANG_0311), choice.svalue, name);
						continue;
					}

					int val = choice.ivalue;
					int bit = 0;
					while (val >>= 1)
					{
						bit++;
					}

					if (bit > 31)
					{
						print_log(get_localized_string(LANG_0312), choice.svalue, name);
					}
					else
					{
						classes[i]->spawnFlagNames[bit] = choice.name;
						classes[i]->spawnFlagDescriptions[bit] = choice.fullDescription;

						bool flgnameexists = false;

						for (auto& s : existsFlagNames)
						{
							if (s == choice.name)
								flgnameexists = true;
						}

						if (!flgnameexists)
						{
							existsFlagNames.push_back(choice.name);
							existsFlagNamesBits.push_back(bit);
						}
					}
				}
			}
		}
	}
}

std::vector<std::string> existsFlagNames;
std::vector<int> existsFlagNamesBits;