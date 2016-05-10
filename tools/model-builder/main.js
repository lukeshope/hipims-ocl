#!/usr/bin/env node
'use strict';

var program = require('commander');
var extent = require('./extent');
var boundaries = require('./boundaries');
var model = require('./model');

function triggerErrorFail(problem) {
	console.log('\n--------------');
	console.log('An error occured:');
	console.log('  ' + problem);
	console.log('\n\nRun with --help for usage.');
	process.exit(1);
}

function getSeconds (timeString) {
	if (typeof timeString !== 'string') {
		console.log('Cannot convert time: ' + timeString);
		return false;
	}
	
	let timePartRegex = /([0-9.]+)([a-z\/ ]+)$/i;
	let parts = timeString.match(timePartRegex);
	let quantity = parts[1];
	let units = parts[2].toLowerCase().trim();
	
	let unitMultiplier;
	if (units === '' || 
	    units === 's' || 
		units === 'sec' || 
		units === 'secs' || 
		units === 'second' ||
		units === 'seconds') 
	{
		unitMultiplier = 1;
	}
	else if (units === 'm' || 
		units === 'min' || 
		units === 'mins' || 
		units === 'minute' ||
		units === 'minutes') 
	{
		unitMultiplier = 60;
	}
	else if (units === 'h' || 
		units === 'hour' || 
		units === 'hours') 
	{
		unitMultiplier = 3600;
	}
	else if (units === 'd' || 
		units === 'day' || 
		units === 'days') 
	{
		unitMultiplier = 3600 * 24;
	}
	
	if (isNaN(quantity)) return false;
	return parseFloat(quantity) * unitMultiplier;
}

function getRate (timeString) {
	if (typeof timeString !== 'string') {
		console.log('Cannot convert rate: ' + timeString);
		return false;
	}
	
	let timePartRegex = /([0-9.]+)([a-z/ ]+)$/i;
	let parts = timeString.match(timePartRegex);
	let quantity = parts[1];
	let units = parts[2].toLowerCase().trim();
	
	// TODO: Support more units than just mm/hr
	
	return parseFloat(quantity);
}

function getInfo (commands) {
	if (!commands.source) return false;
	if (!commands.directory) return false;
	
	var modelSource = commands.source.toString().toLowerCase();
	
	if (modelSource !== 'pluvial') {
		console.log('Sorry -- only pluvial models are currently supported by model builder.');
		return false;
	}
	
	if (commands.resolution !== undefined) {
		console.log('Sorry -- resampling to a different resolution not yet supported.');
		return false;
	}
	
	if (commands.decompose !== undefined) {
		console.log('Sorry -- domain decomposition not yet supported.');
		return false;
	}
	
	return {
		name: commands.name || 'Undefined',
		source: modelSource,
		targetDirectory: commands.directory,
		duration: getSeconds(commands.time)
	};
}

function getExtent (commands) {
	if (!commands.lowerLeft || !commands.upperRight) {
		console.log('You must supply a model extent.');
		return false;
	}
	
	var llCoords = commands.lowerLeft.split(',');
	var urCoords = commands.upperRight.split(',');
	
	if (llCoords.length != 2 ||
	    urCoords.length != 2 ||
		isNaN(llCoords[0]) ||
		isNaN(llCoords[1]) ||
		isNaN(urCoords[0]) ||
		isNaN(urCoords[1])) {
		console.log('Coordinates supplied are invalid.');
		return false;
	}
	
	return new extent(llCoords[0], llCoords[1], urCoords[0], urCoords[1]);
}

function getBoundaries (modelInfo, commands) {
	if (modelInfo.source === 'pluvial') {
		let rainfallIntensity = getRate(commands.rainfallIntensity);
		let rainfallDuration = getSeconds(commands.rainfallDuration);
		let drainageRate = getRate(commands.drainage);
		
		if (rainfallIntensity === false) {
			console.log('Rainfall intensity invalid or not provided.');
			return false;
		}
		if (rainfallDuration === false) {
			console.log('Rainfall duration invalid or not provided.');
			return false;
		}
		if (drainageRate === false) {
			console.log('Drainage rate invalid or not provided.');
			return false;
		}
		
		return new boundaries({
			rainfallIntensity: rainfallIntensity,
			rainfallDuration: rainfallDuration,
			drainageRate: drainageRate
		});
	} else {
		console.log('Cannot prepare boundaries for this type of model.');
		return false;
	}
}

program
	.version('0.0.1')
	.option('-n, --name <name>', 'short name for the model')
	.option('-s, --source [pluvial|fluvial|tidal|combined]', 'type of model to construct')
	.option('-d, --directory <dir>', 'target directory for model')
	.option('-r, --resolution <resolution>', 'grid resolution in metres')
	.option('-t, --time <duration>', 'duration of simulation')
	.option('-dn, --decompose <domains>', 'decompose for multi-device')
	.option('-ll, --lower-left <easting,northing>', 'lower left coordinates')
	.option('-ur, --upper-right <easting,northing>', 'upper right coordinates')
	.option('-ri, --rainfall-intensity <Xmm/hr>', 'rainfall intensity')
	.option('-rd, --rainfall-duration <Xmins>', 'rainfall duration')
	.option('-dr, --drainage <Xmm/hr>', 'drainage rate')
	.parse(process.argv)
	
var modelInfo = getInfo(program);
if (!modelInfo) triggerErrorFail('You must specify more inforation about this model.');

var modelExtent = getExtent(program);
if (!modelExtent) triggerErrorFail('You must specify a valid extent for the model.');

var modelBoundaries = getBoundaries(modelInfo, program);
if (!modelBoundaries) triggerErrorFail('You must specify more boundary conditions.');

var model = new model(modelInfo, modelExtent, modelBoundaries);
model.prepareModel( (success) => {
	if (success) {
		model.outputModel();
	}
});


// TODO: Cookie cut files using shapefile for buildings
// TODO: Rearrange and store files in the right directories
// TODO: Write out the model XML config file
