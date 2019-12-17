#!/usr/bin/env python3

import copy
import os
from pathlib import Path
import shutil
import subprocess
from xml.etree import ElementTree
import uuid

if os.path.isdir('vsproject'):
	shutil.rmtree('vsproject')
if os.path.isdir('vsproject-temp-debug'):
	shutil.rmtree('vsproject-temp-debug')
if os.path.isdir('vsproject-temp-release'):
	shutil.rmtree('vsproject-temp-release')
if os.path.isdir('vsproject-temp-static'):
	shutil.rmtree('vsproject-temp-static')
os.makedirs('vsproject')

subprocess.run([
	'meson',
	'--backend=vs',
	'-Db_pch=false',
	'vsproject-temp-debug',
], check = True)
subprocess.run([
	'meson',
	'--backend=vs',
	'-Dbuildtype=release',
	'-Dignore_updates=false',
	'-Dinstall_check=true',
	'vsproject-temp-release',
], check = True)
subprocess.run([
	'meson',
	'--backend=vs',
	'-Dbuildtype=release',
	'-Dignore_updates=false',
	'-Dinstall_check=true',
	'-Db_vscrt=mt',
	'-Db_lto=true',
	'-Dstatic=prebuilt',
	'vsproject-temp-static',
], check = True)

os.makedirs(os.path.join('vsproject', 'src'))
for header in [ 'Config.h', 'ElementNumbers.h', 'ToolNumbers.h' ]:
	shutil.copy(os.path.join('vsproject-temp-release', 'src', header), os.path.join('vsproject', 'src', header))
for basename in os.listdir('vsproject-temp-debug'):
	if basename.endswith('.dll'):
		shutil.copy(os.path.join('vsproject-temp-debug', basename), os.path.join('vsproject', basename))

NS_MSBUILD = 'http://schemas.microsoft.com/developer/msbuild/2003'
def default_ns(tag):
	return '{{{0}}}{1}'.format(NS_MSBUILD, tag)
NAMESPACES = { '': NS_MSBUILD }
for key, value in NAMESPACES.items():
	ElementTree.register_namespace(key, value)
factored_out = [ default_ns('AdditionalOptions'), default_ns('PreprocessorDefinitions'), default_ns('AdditionalIncludeDirectories'), default_ns('ObjectFileName') ]

input_projects = {}
for config in [ 'debug', 'release', 'static' ]:
	input_projects[config] = ElementTree.parse(os.path.join('vsproject-temp-' + config, 'powder@exe.vcxproj')).getroot()

output_project = copy.deepcopy(input_projects['debug'])
while len(output_project):
	output_project.remove(output_project[0])

output_filters = copy.deepcopy(output_project)
del output_filters.attrib['DefaultTargets']

sources_used = set()
for property_group in input_projects['debug'].findall('./PropertyGroup', namespaces = NAMESPACES):
	if 'Label' in property_group.attrib:
		labelled_pg_clone = copy.deepcopy(property_group)
		output_project.append(labelled_pg_clone)
	else:
		for config in [ 'debug', 'release', 'static' ]:
			unlabelled_pg_clone = copy.deepcopy(property_group)
			unlabelled_pg_clone.attrib['Condition'] = '\'$(Configuration)|$(Platform)\'==\'' + config + '|x64\''
			unlabelled_pg_clone.find('./IntDir', namespaces = NAMESPACES).text = 'build\\' + config + '\\objects\\'
			unlabelled_pg_clone.find('./OutDir', namespaces = NAMESPACES).text = 'build\\' + config + '\\'
			output_project.append(unlabelled_pg_clone)
for project_configs in input_projects['debug'].findall('./ItemGroup[@Label="ProjectConfigurations"]', namespaces = NAMESPACES):
	proj_configs = copy.deepcopy(project_configs)
	output_project.append(proj_configs)
for project_configs in input_projects['release'].findall('./ItemGroup[@Label="ProjectConfigurations"]/*', namespaces = NAMESPACES):
	proj_configs.append(copy.deepcopy(project_configs))
for project_configs in input_projects['static'].findall('./ItemGroup[@Label="ProjectConfigurations"]/*', namespaces = NAMESPACES):
	proj_configs_static = copy.deepcopy(project_configs)
	proj_configs_static.attrib['Include'] = "static|x64"
	proj_configs_static.find('./Configuration', namespaces = NAMESPACES).text = "static"
	proj_configs.append(proj_configs_static)
for imports in input_projects['debug'].findall('./Import', namespaces = NAMESPACES):
	output_project.append(copy.deepcopy(imports))
for item_group in input_projects['debug'].findall('./ItemGroup', namespaces = NAMESPACES):
	if 'Label' in item_group.attrib and item_group.attrib['Label'] == 'ProjectConfigurations':
		continue
	if item_group.find('./Object', namespaces = NAMESPACES) != None:
		continue
	if item_group.find('./None[@Include=\'..\\meson.build\']', namespaces = NAMESPACES) != None:
		continue
	if item_group.find('./ProjectReference', namespaces = NAMESPACES) != None:
		continue
	project_reference = item_group.find('./ProjectReference', namespaces = NAMESPACES)
	if project_reference and 'REGEN' in project_reference.attrib['Include']:
		continue
	ig_clone = copy.deepcopy(item_group)
	for clcompile in ig_clone.findall('./CLCompile', namespaces = NAMESPACES):
		clcompile.attrib['Include'] = clcompile.attrib['Include'].replace('/', '\\')
		sources_used.add(clcompile.attrib['Include'])
		to_remove = []
		for clcompile_option in clcompile.findall('./*'):
			if clcompile_option.tag in factored_out:
				to_remove.append(clcompile_option)
		for clcompile_option in to_remove:
			clcompile.remove(clcompile_option)
	output_project.append(ig_clone)
for config in [ 'debug', 'release', 'static' ]:
	factored_out_values = {}
	for item_group in input_projects[config].findall('./ItemGroup', namespaces = NAMESPACES):
		for clcompile in item_group.findall('./CLCompile', namespaces = NAMESPACES):
			for clcompile_option in clcompile.findall('./*'):
				if clcompile_option.tag in factored_out:
					factored_out_values[clcompile_option.tag] = clcompile_option.text
			break
	for itemdef_group in input_projects[config].findall('./ItemDefinitionGroup', namespaces = NAMESPACES):
		idg_clone = copy.deepcopy(itemdef_group)
		for tag in factored_out:
			clcompile_option = idg_clone.find('./ClCompile/' + tag, namespaces = NAMESPACES)
			if clcompile_option != None:
				option_name = '%({0})'.format(tag.split('}')[1])
				clcompile_option.text = clcompile_option.text.replace(option_name, factored_out_values[tag])
				if option_name == '%(PreprocessorDefinitions)' and config == 'debug':
					clcompile_option.text += ';DEBUG;IGNORE_UPDATES;NO_INSTALL_CHECK'
				if option_name == '%(PreprocessorDefinitions)' and config == 'static':
					clcompile_option.text += ';CURL_STATICLIB;ZLIB_WINAPI'
		idg_clone.attrib['Condition'] = '\'$(Configuration)|$(Platform)\'==\'' + config + '|x64\''
		object_file_name = ElementTree.Element('ObjectFileName')
		object_file_name.text = '$(IntDir)/%(RelativeDir)/'
		idg_clone.find('./ClCompile', namespaces = NAMESPACES).append(object_file_name)
		additional_options = ElementTree.Element('AdditionalOptions')
		additional_options.text = factored_out_values[default_ns('AdditionalOptions')] + ' "/MP"'
		idg_clone.find('./ClCompile', namespaces = NAMESPACES).append(additional_options)
		resource_compile = ElementTree.Element('ResourceCompile')
		resource_compile_projdef = ElementTree.Element('PreprocessorDefinitions')
		resource_compile_projdef.text = '%(PreprocessorDefinitions)'
		resource_compile.append(resource_compile_projdef)
		resource_compile_incdirs = ElementTree.Element('AdditionalIncludeDirectories')
		resource_compile_incdirs.text = '$(IntDir);%(AdditionalIncludeDirectories)'
		resource_compile.append(resource_compile_incdirs)
		idg_clone.append(resource_compile)
		output_project.append(idg_clone)
ig_resource_compile = ElementTree.Element('ItemGroup')
ig_resource_compile_powder_res = ElementTree.Element('ResourceCompile')
ig_resource_compile_powder_res.attrib['Include'] = '..\\resources\\powder-res.rc'
ig_resource_compile.append(ig_resource_compile_powder_res)
output_project.append(ig_resource_compile)

cl_compile = []
cl_include = []
source_dirs = set()
for root, subdirs, files in os.walk('src'):
	for file in [ os.path.join(root, f) for f in files ]:
		lowerfile = file.lower()
		add_source_dir = False
		if lowerfile.endswith('.cpp') or lowerfile.endswith('.c'):
			cl_compile.append(file)
			add_source_dir = True
		if lowerfile.endswith('.hpp') or lowerfile.endswith('.h'):
			cl_include.append(file)
			add_source_dir = True
		if add_source_dir:
			path = Path(root)
			for i in range(len(path.parents) - 1):
				parent = str(path.parents[i])
				if not parent in source_dirs:
					source_dirs.add(parent)
			source_dirs.add(os.path.dirname(file))

filter_records = ElementTree.Element('ItemGroup')
for d in source_dirs | set([ 'data', 'resources' ]):
	filter_record = ElementTree.Element('Filter')
	filter_record.attrib['Include'] = d
	filter_record_id = ElementTree.Element('UniqueIdentifier')
	filter_record_id.text = '{{{0}}}'.format(str(uuid.uuid4()))
	filter_record.append(filter_record_id)
	filter_records.append(filter_record)
output_filters.append(filter_records)

nones_in_filters = ElementTree.Element('ItemGroup')
clcompiles_in_filters = ElementTree.Element('ItemGroup')
extra_nones_in_project = ElementTree.Element('ItemGroup')
for real_path, filter_path in {
	'..\\data\\font.cpp': 'data\\font.cpp',
	'..\\data\\hmap.cpp': 'data\\hmap.cpp',
	'..\\data\\icon.cpp': 'data\\icon.cpp',
	'..\\data\\images.cpp': 'data\\images.cpp',
	'..\\resources\\powder-res.rc': 'resources\\powder-res.rc',
	'src\\Config.h': 'src\\Config.h',
	'src\\ToolNumbers.h': 'src\\simulation\\ToolNumbers.h',
	'src\\ElementNumbers.h': 'src\\simulation\\ElementNumbers.h',
}.items():
	filter_record = ElementTree.Element(real_path.endswith('.cpp') and 'ClCompile' or (real_path.endswith('.h') and 'ClInclude' or 'ResourceCompile'))
	filter_record.attrib['Include'] = real_path
	filter_record_path = ElementTree.Element('Filter')
	filter_record_path.text = os.path.dirname(filter_path)
	filter_record.append(filter_record_path)
	nones_in_filters.append(filter_record)
for f in cl_compile:
	used = ('..\\' + f).replace('/', '\\') in sources_used
	if used:
		filter_record = ElementTree.Element('CLCompile')
		filter_record.attrib['Include'] = '..\\' + f
		clcompiles_in_filters.append(filter_record)
	else:
		extranone = ElementTree.Element('None')
		extranone.attrib['Include'] = '..\\' + f
		extra_nones_in_project.append(extranone)
		filter_record = ElementTree.Element('None')
		filter_record.attrib['Include'] = '..\\' + f
		nones_in_filters.append(filter_record)
	filter_record_path = ElementTree.Element('Filter')
	filter_record_path.text = os.path.dirname(f)
	filter_record.append(filter_record_path)
output_filters.append(clcompiles_in_filters)
output_filters.append(nones_in_filters)
output_project.append(extra_nones_in_project)

configured_header = {
	'..\\src\\Config.template.h': 'src\\Config.h',
	'..\\src\\simulation\\ToolNumbers.template.h': 'src\\ToolNumbers.h',
	'..\\src\\simulation\\ElementNumbers.template.h': 'src\\ElementNumbers.h',
}
clincludes_in_filters = ElementTree.Element('ItemGroup')
clincludes_in_project = ElementTree.Element('ItemGroup')
for f in cl_include:
	filter_record = ElementTree.Element('ClInclude')
	cl_include_path = '..\\' + f
	if cl_include_path in configured_header:
		cl_include_path = configured_header[cl_include_path]
	filter_record.attrib['Include'] = cl_include_path
	clinclude = copy.deepcopy(filter_record)
	filter_record_path = ElementTree.Element('Filter')
	filter_record_path.text = os.path.dirname(f)
	filter_record.append(filter_record_path)
	clincludes_in_filters.append(filter_record)
	clincludes_in_project.append(clinclude)
output_filters.append(clincludes_in_filters)
output_project.append(clincludes_in_project)

ElementTree.ElementTree(output_project).write(os.path.join('vsproject' , 'powder.vcxproj'), xml_declaration = True, encoding = 'utf-8', method = 'xml')

ElementTree.ElementTree(output_filters).write(os.path.join('vsproject' , 'powder.vcxproj.filters'), xml_declaration = True, encoding = 'utf-8', method = 'xml')

with open(os.path.join('vsproject' , 'the-powder-toy.sln'), 'w') as sln:
	sln.write("""Microsoft Visual Studio Solution File, Format Version 11.00
# Visual Studio 2017
Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "powder", "powder.vcxproj", "{0}"
EndProject
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		debug|x64 = debug|x64
		release|x64 = release|x64
		static|x64 = static|x64
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		{0}.debug|x64.ActiveCfg = debug|x64
		{0}.debug|x64.Build.0 = debug|x64
		{0}.release|x64.ActiveCfg = release|x64
		{0}.release|x64.Build.0 = release|x64
		{0}.static|x64.ActiveCfg = static|x64
		{0}.static|x64.Build.0 = static|x64
	EndGlobalSection
	GlobalSection(SolutionProperties) = preSolution
		HideSolutionNode = FALSE
	EndGlobalSection
EndGlobal
""".format(output_project.find('./PropertyGroup[@Label=\'Globals\']/ProjectGuid', namespaces = NAMESPACES).text))

config_h = os.path.join('vsproject', 'src', 'Config.h')
with open(config_h) as config:
	config_str = config.read()
for nuke in [ 'CURL_STATICLIB', 'ZLIB_WINAPI', 'DEBUG', 'IGNORE_UPDATES', 'NO_INSTALL_CHECK' ]:
	config_str = config_str.replace('#undef {0}'.format(nuke), '')
with open(config_h, 'w') as config:
	config.write(config_str)

with open(os.path.join('vsproject', '.gitignore'), 'w') as ign:
	ign.write(""".vs/
build/
*.vcxproj.user
""")

shutil.rmtree('vsproject-temp-debug')
shutil.rmtree('vsproject-temp-release')
shutil.rmtree('vsproject-temp-static')
