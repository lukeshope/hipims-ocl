#!/usr/bin/env node
'use strict';

var program = require('commander');
var extent = require('./extent');
var model = require('./model');

function triggerErrorFail(problem) {
	console.log('\n\n--------------');
	console.log('An error occured:');
	console.log('  ' + problem);
	console.log('\n\nRun with --help for usage.');
	process.exit(1);
}

function getInfo (commands) {
	if (!commands.source) return false;
	if (!commands.directory) return false;
	
	var modelSource = commands.source.toString().toLowerCase();
	
	if (modelSource !== 'pluvial') {
		triggerErrorFail('Sorry -- only pluvial models are currently supported.');
	}
	
	return {
		name: commands.name || 'Undefined',
		source: modelSource,
		targetDirectory: commands.directory
	};
}

function getExtent (commands) {
	if (!commands.lowerLeft) return false;
	if (!commands.upperRight) return false;
	
	var llCoords = commands.lowerLeft.split(',');
	var urCoords = commands.upperRight.split(',');
	
	if (llCoords.length != 2 ||
	    urCoords.length != 2 ||
		isNaN(llCoords[0]) ||
		isNaN(llCoords[1]) ||
		isNaN(urCoords[0]) ||
		isNaN(urCoords[1])) {
		return false;
	}
	
	return new extent(llCoords[0], llCoords[1], urCoords[0], urCoords[1]);
}

program
	.version('0.0.1')
	.option('-n, --name <name>', 'short name for the model')
	.option('-s, --source [pluvial|fluvial|tidal|combined]', 'type of model to construct')
	.option('-d, --directory <dir>', 'target directory for model')
	.option('-r, --resolution <resolution>', 'grid resolution in metres')
	.option('-dn, --decompose <domains>', 'decompose to a number of domain')
	.option('-ll, --lower-left <easting,northing>', 'lower left coordinates')
	.option('-ur, --upper-right <easting,northing>', 'upper right coordinates')
	.option('-ri, --rainfall-intensity <mm/hr>', 'rainfall intensity', parseFloat)
	.option('-rd, --rainfall-duration <minutes>', 'rainfall duration', parseFloat)
	.option('-d, --drainage <mm/hr>', 'drainage network capacity', parseFloat)
	.parse(process.argv)
	
// Identify the extent and LiDAR tiles required
var modelInfo = getInfo(program);
var modelExtent = getExtent(program);
if (!modelExtent) triggerErrorFail('You must specify a valid extent for the model.');
if (!modelInfo) triggerErrorFail('You must specify more information about this model.');

var model = new model(modelInfo.name, modelInfo.targetDirectory, modelInfo.source, modelExtent);
model.prepareModel();

// TODO: Cookie cut files using shapefile for buildings
// TODO: Rearrange and store files in the right directories
// TODO: Write out the model XML config file
