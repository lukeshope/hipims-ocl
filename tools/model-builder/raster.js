'use strict';

var gdal = require('gdal');
var fs = require('fs');
var path = require('path');

var rasterTools = function() {
	// ...
};

rasterTools.prototype.mergeToVRT = function (targetVRT, sourceFiles, cb) {
	// We don't use node-gdal here because we can't access the individual
	// XML elements that way.
	const dataType = 'Float32';
	const noDataValue = -9999;
	const vrtBaseDir = path.dirname(targetVRT);
	
	var fileData = {};
	var fileTotal = sourceFiles.length;
	var fileCount = 0;
	
	// Get each file's geotransforms and sizes
	sourceFiles.map((fn) => {
		var dataset = gdal.open(fn);
		
		if (!dataset) {
			console.log('Could not open "' + fn + '" to add to VRT dataset.');
			return '';
		}
		
		var sizeX = dataset.rasterSize.x;
		var sizeY = dataset.rasterSize.y;
		var transform = dataset.geoTransform;
		var bandCount = dataset.bands.count();
		dataset.close();
	
		var lowerLeftX = transform[0];
		var lowerLeftY = transform[5] < 0 ? transform[3] - sizeY * Math.abs(transform[5]) : transform[3];
		var upperRightX = transform[0] + sizeX * transform[1];
		var upperRightY = transform[5] > 0 ? transform[3] + sizeY * Math.abs(transform[5]) : transform[3];

		fileData[fn] = {
			sizeX: sizeX,
			sizeY: sizeY,
			transform: transform,
			bandCount: bandCount,
			lowerLeftX: lowerLeftX,
			lowerLeftY: lowerLeftY,
			upperRightX: upperRightX,
			upperRightY: upperRightY,
			offsetX: null,
			offsetY: null,
			offsetBaseNorth: transform[5] < 0,
			resolution: Math.abs(transform[5])
		};
	});
	
	// Get the whole set's min and max coordinates
	var setMinX = Infinity;
	var setMaxX = -Infinity;
	var setMinY = Infinity;
	var setMaxY = -Infinity;
	var setResolution = Infinity;
	var setBaseNorth = null;
	
	Object.keys(fileData).forEach((fn) => { 
		setMinX = Math.min(fileData[fn].lowerLeftX, setMinX);
		setMinY = Math.min(fileData[fn].lowerLeftY, setMinY);
		setMaxX = Math.max(fileData[fn].upperRightX, setMaxX);
		setMaxY = Math.max(fileData[fn].upperRightY, setMaxY);
		setResolution = Math.min(fileData[fn].resolution, setResolution);
		if (setBaseNorth === null) {
			setBaseNorth = fileData[fn].offsetBaseNorth;
		} else {
			if (setBaseNorth != fileData[fn].offsetBaseNorth) {
				console.log('Different Y resolutions found -- tool does not support this yet.');
			}
		}
	});
	
	if (setMinX === Infinity || 
	    setMinY === Infinity ||
		setMaxX === -Infinity ||
		setMaxY === -Infinity ||
		setBaseNorth === null ||
		setResolution === Infinity) {
		console.log('Could not compute maximum dimensions for the merged raster.');
	} else {
		console.log('VRT stretches over extent (' + setMinX + ', ' + setMinY + ') to (' + setMaxX + ', ' + setMaxY + ').');
	}
	
	// Calculate relative positions for each dataset
	Object.keys(fileData).forEach((fn) => { 
		fileData[fn].offsetX = (fileData[fn].lowerLeftX - setMinX) / fileData[fn].resolution;
		fileData[fn].offsetY = (fileData[fn].offsetBaseNorth ? (setMaxY - fileData[fn].upperRightY) : (fileData[fn].lowerLeftY - setMinY)) / fileData[fn].resolution;
	});
	
	var vrtHeader = '<VRTDataset rasterXSize="' + ((setMaxX - setMinX)/setResolution) + '" rasterYSize="' + ((setMaxY - setMinY)/setResolution) + '">\r\n\
		<GeoTransform>\r\n\
			' + setMinX + ', \
			' + setResolution + ', \
			0.0, \
			' + ( setBaseNorth ? setMaxY : setMinY ) + ', \
			0.0, \
			' + ( setBaseNorth ? -setResolution : setResolution ) + ' \
		</GeoTransform>\r\n\
		<VRTRasterBand dataType="' + dataType + '" band="1">\r\n\
			<NoDataValue>' + noDataValue + '</NoDataValue>';
	var vrtFooter = '\
		</VRTRasterBand>\r\n\
	</VRTDataset>';
	var vrtBody = '';
	
	Object.keys(fileData).forEach((fn) => { 
		vrtBody += this.getVRTBandForFile(
			fn, 
			vrtBaseDir, 
			fileData[fn],
			dataType,
			noDataValue
		); 
		fileCount++;
		
		if (fileCount >= fileTotal) {
			console.log('Writing to ' + targetVRT + '...');
			fs.writeFile(
				targetVRT,
				vrtHeader + '\r\n' + vrtBody + '\r\n' + vrtFooter,
				(err) => {
					if (cb) cb(err);
				}
			);
		}
	});
};

rasterTools.prototype.getVRTBandForFile = function (filename, baseDir, data, dataType, noDataValue) {
	return '\
		<SimpleSource>\r\n\
		  <SourceFilename relativeToVRT="1">' + path.relative(baseDir, filename) + '</SourceFilename>\r\n\
		  <SourceBand>1</SourceBand>\r\n\
		  <SourceProperties RasterXSize="' + data.sizeX + '" RasterYSize="' + data.sizeY + '" DataType="' + dataType + '" BlockXSize="' + data.sizeX + '" BlockYSize="1" />\r\n\
		  <SrcRect xOff="0" yOff="0" xSize="' + data.sizeX + '" ySize="' + data.sizeX + '" />\r\n\
		  <DstRect xOff="' + data.offsetX + '" yOff="' + data.offsetY + '" xSize="' + data.sizeY + '" ySize="' + data.sizeY + '" />\r\n\
		  <NODATA>' + noDataValue + '</NODATA>\r\n\
		</SimpleSource>\r\n';
}

var thisInstance = null;
module.exports = function () {
	if ( !thisInstance ) {
		thisInstance = new rasterTools();
	}
	return thisInstance;
}();
