#!/usr/bin/env node
'use strict';

var commands = require('commander');
var domain = require('./domain');

commands
	.version('0.0.1')
	.command('hipims-mb')
	.option('-n --name <name>', 'short name for the model')
	.option('-t --type pluvial|fluvial|tidal|combined', 'type of model to construct')
	.option('-d --directory <dir>', 'target directory for model')
	.option('-r --resolution <resolution>', 'grid resolution in metres')
	.option('-dn --decompose <domains>', 'decompose to a number of domain')
	.option('-ll --lower-left <easting,northing>', 'lower left coordinates')
	.option('-ur --upper-right <easting,northing>', 'upper right coordinates')
	.option('-ri --rainfall-intensity <mm/hr>', 'rainfall intensity', parseFloat)
	.option('-rd --rainfall-duration <minutes>', 'rainfall duration', parseFloat)
	.option('-d --drainage <mm/hr>', 'drainage network capacity', parseFloat)
	.parse(process.argv);

// TODO: Add extent class
// TODO: Identify required BNG tiles from the extent
var modelExtent = null;
var modelDomain = new domain(modelExtent);

// TODO: Cookie cut files using shapefile for buildings
// ...

// TODO: Rearrange and store files in the right directories
// ...

// TODO: Write out the model XML config file
// ...
