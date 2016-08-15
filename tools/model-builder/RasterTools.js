'use strict';

var gdal = require('gdal');
var fs = require('fs');
var path = require('path');
var Extent = require('./Extent');

function RasterTools () {
	// ...
};

RasterTools.prototype.mergeToVRT = function (targetVRT, sourceFiles, cb) {
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
			console.log('    Could not open "' + fn + '" to add to VRT dataset.');
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
				console.log('    Different Y resolutions found -- tool does not support this yet.');
			}
		}
	});
	
	if (setMinX === Infinity || 
	    setMinY === Infinity ||
		setMaxX === -Infinity ||
		setMaxY === -Infinity ||
		setBaseNorth === null ||
		setResolution === Infinity) {
		console.log('    Could not compute maximum dimensions for the merged raster.');
	} else {
		console.log('    VRT stretches over extent (' + setMinX + ', ' + setMinY + ') to (' + setMaxX + ', ' + setMaxY + ').');
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
			console.log('    Writing to ' + targetVRT + '...');
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

RasterTools.prototype.getVRTBandForFile = function (filename, baseDir, data, dataType, noDataValue) {
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

RasterTools.prototype.divideRaster = function (sourceFile, targetFiles, overlapRows, cb) {
	let sourceDataset = gdal.open(sourceFile);
	var targetCount = targetFiles.length;
	
	if (!sourceDataset) {
		console.log('    Could not open "' + sourceFile + '" to divide data.');
		return false;
	}
	
	let sourceSizeX = sourceDataset.rasterSize.x;
	let sourceSizeY = sourceDataset.rasterSize.y;
	let sourceTransform = sourceDataset.geoTransform;
	let sourceResolution = Math.abs(sourceTransform[5]);
	let sourceBaseX = sourceTransform[0];
	let sourceBaseY = sourceTransform[5] < 0 ? sourceTransform[3] - sourceSizeY * Math.abs(sourceTransform[5]) : sourceTransform[3];
	let sourceFullSize = (sourceSizeY + (targetCount - 1) * overlapRows) * sourceResolution;
	let sourceEachSize = sourceFullSize / targetCount;

	let clipXMin = sourceBaseX;
	let clipXMax = sourceBaseX + sourceSizeX * sourceResolution;
	let clipComplete = 0;
	for (let i = 0; i < targetCount; i++) {
		let clipYMin = Math.floor((sourceBaseY + i * sourceEachSize - i * overlapRows * sourceResolution)/sourceResolution) * sourceResolution;
		let clipYMax = Math.ceil((clipYMin + sourceEachSize)/sourceResolution) * sourceResolution;
		this.clipRaster(
			sourceFile,
			targetFiles[i],
			'HFA',
			new Extent(
				clipXMin, 
				clipYMin,
				clipXMax,
				clipYMax
			),
			(success) => {
				if (!success) return;
				clipComplete++;
				if (clipComplete >= targetCount) {
					cb(true);
				}
			}
		);
	}
}

RasterTools.prototype.clipRaster = function (sourceFile, targetFile, format, extent, cb) {
	let sourceDataset = gdal.open(sourceFile);
	
	if (!sourceDataset) {
		console.log('    Could not open "' + fn + '" to source clip data.');
		return false;
	}
	
	// Fetch everything we need to know about the source 
	// except the data itself.
	let sourceSizeX = sourceDataset.rasterSize.x;
	let sourceSizeY = sourceDataset.rasterSize.y;
	let sourceTransform = sourceDataset.geoTransform;
	let sourceBandCount = sourceDataset.bands.count();
	let sourceResolution = Math.abs(sourceTransform[5]);
	
	let sourceBaseX = sourceTransform[0];
	let sourceBaseY = sourceTransform[5] < 0 ? sourceTransform[3] - sourceSizeY * Math.abs(sourceTransform[5]) : sourceTransform[3];
	let sourceCornerX = sourceTransform[0] + sourceSizeX * sourceTransform[1];
	let sourceCornerY = sourceTransform[5] > 0 ? sourceTransform[3] + sourceSizeY * Math.abs(sourceTransform[5]) : sourceTransform[3];

	let clipSourceMinX = Math.min(sourceSizeX, Math.max(0, Math.floor((extent.lowerLeftX - sourceBaseX)/sourceResolution)));
	let clipSourceMinY = Math.min(sourceSizeY, Math.max(0, Math.floor((extent.lowerLeftY - sourceBaseY)/sourceResolution)));
	let clipSourceMaxX = Math.min(sourceSizeX, Math.max(0, Math.ceil((extent.upperRightX - sourceBaseX)/sourceResolution)));
	let clipSourceMaxY = Math.min(sourceSizeY, Math.max(0, Math.ceil((extent.upperRightY - sourceBaseY)/sourceResolution)));

	const dataType = 'Float32';
	const noDataValue = -9999;
	const windowSize = 32;
	const targetBaseDir = path.dirname(targetFile);
	const targetDriver = gdal.drivers.get(format);
	const targetSizeX = Math.abs(clipSourceMaxX - clipSourceMinX);
	const targetSizeY = Math.abs(clipSourceMaxY - clipSourceMinY);
	
	console.log('    Target file will be ' + targetSizeX + 'x' + targetSizeY + ' (' + (targetSizeX * targetSizeY) + ' cells)');
	
	if (!targetDriver) {
		console.log('    Could not obtain driver "' + format + '" to create raster file.');
		return false;
	}
	
	let targetDataset = targetDriver.create(
		targetFile,
		targetSizeX,
		targetSizeY,
		1,
		dataType
	);
	
	if (!targetDataset) {
		console.log('    Could not create dataset for clip.');
		if (cb) cb(false);
		return false;
	}
	
	// Set spatial reference info
	let targetTransform = [
		Math.floor(extent.lowerLeftX / sourceResolution) * sourceResolution,
		sourceResolution,
		0.0,
		Math.floor(( sourceTransform[5] > 0.0 ? extent.lowerLeftY : extent.upperRightY ) / sourceResolution) * sourceResolution,
		0.0,
		sourceTransform[5]
	];
	targetDataset.geoTransform = targetTransform;
	
	// Copy data
	let sourceBand = sourceDataset.bands.get(1);
	let targetBand = targetDataset.bands.get(1);
	
	if (!sourceBand) {
		console.log('    Could not get source band for clip.');
		if (cb) cb(false);
		return false;
	}
	if (!targetBand) {
		console.log('    Could not get target band for clip.');
		if (cb) cb(false);
		return false;
	}
	
	let sourcePixels = sourceBand.pixels;
	let targetPixels = targetBand.pixels;
	
	targetBand.noDataValue = noDataValue;
	
	for (let e = 0; e < Math.ceil(targetSizeX / windowSize) * windowSize; e += windowSize ) {
		for (let n = 0; n < Math.ceil(targetSizeY / windowSize) * windowSize; n += windowSize ) {
			let maxE = Math.min(targetSizeX, e + windowSize);
			let maxN = Math.min(targetSizeY, n + windowSize);
			
			let sourceBuffer = new Float32Array(new ArrayBuffer((maxE - e) * (maxN - n) * 4));
			
			sourcePixels.read(clipSourceMinX + e, sourceSizeY - (clipSourceMinY + n) - (maxN - n), maxE - e, maxN - n, sourceBuffer);
			targetPixels.write(e, targetSizeY - n - (maxN - n), maxE - e, maxN - n, sourceBuffer);
		}
	}
	
	targetDataset.flush();
	targetDataset.close();
	
	sourceDataset.close();
	
	if (cb) cb(true);
	return true;
};


RasterTools.prototype.arrayToRaster = function (targetFile, format, extent, resolution, outputArray, cb) {
	const dataType = 'Float32';
	const noDataValue = -9999;
	const targetBaseDir = path.dirname(targetFile);
	const targetDriver = gdal.drivers.get(format);
	
	extent.snapToGrid(resolution);
	const targetSizeX = extent.getSizeX(resolution);
	const targetSizeY = extent.getSizeY(resolution);
	
	console.log('    Target file will be ' + targetSizeX + 'x' + targetSizeY + ' (' + (targetSizeX * targetSizeY) + ' cells)');
	
	if (!targetDriver) {
		console.log('    Could not obtain driver "' + format + '" to create raster file.');
		return false;
	}
	
	let targetDataset = targetDriver.create(
		targetFile,
		targetSizeX,
		targetSizeY,
		1,
		dataType
	);
	
	if (!targetDataset) {
		console.log('    Could not create dataset for array output.');
		if (cb) cb(false);
		return false;
	}
	
	let targetTransform = [
		Math.floor(extent.getLowerX() / resolution) * resolution,
		resolution,
		0.0,
		Math.floor(extent.getUpperY() / resolution) * resolution,
		0.0,
		-resolution
	];
	
	targetDataset.geoTransform = targetTransform;
	let targetBand = targetDataset.bands.get(1);

	if (!targetBand) {
		console.log('    Could not get target band for array output.');
		if (cb) cb(false);
		return false;
	}
	
	let targetPixels = targetBand.pixels;
	targetBand.noDataValue = noDataValue;
	for (let y = 0; y < targetSizeY; y++) {
		let rowData = new Float32Array(Array.prototype.slice.call(outputArray, y * targetSizeX, (y + 1) * targetSizeX));
		targetPixels.write(0, targetSizeY - y - 1, targetSizeX, 1, rowData);
	}
	targetDataset.flush();
	targetDataset.close();
	
	if (cb) setTimeout(function () { cb(true) }, 0);
	return true;
};

var thisInstance = null;
module.exports = function () {
	if ( !thisInstance ) {
		thisInstance = new RasterTools();
	}
	return thisInstance;
}();
