'use strict';

const defaultResolution = 2.0;

const fs = require('fs');
const path = require('path');
const domain = require('./Domain');
const downloadTools = require('./DownloadTools');

function Model (definition, extent, boundaries) {
	this.name = definition.name;
	this.duration = definition.duration;
	this.targetDirectory = definition.targetDirectory;
	this.domainType = definition.domainType;
	this.domainResolution = definition.domainResolution || defaultResolution;
	this.extent = extent;
	this.source = definition.source;
	this.outputFrequency = definition.outputFrequency || this.duration;
	this.domain = null;
	this.boundaries = boundaries;
};

Model.prototype.getName = function () {
	return this.name;
}

Model.prototype.getExtent = function () {
	return this.extent;
}

Model.prototype.getResolution = function () {
	return this.domainResolution;
}

Model.prototype.prepareModel = function(cb) {
	let domainClass = domain.getDomainForType(this.domainType);
	this.extent.snapToGrid(this.domainResolution);
	this.domain = new domainClass(this, cb);
};

Model.prototype.outputModel = function() {
	console.log('--> Writing model files...');

	this.outputReady = false;
	this.outputConfiguration = false;
	this.outputTopography = false;
	this.outputBoundaries = false;
	
	fs.access(this.targetDirectory, fs.R_OK | fs.W_OK, (err) => {
		if (err) {
			console.log('    No model directory currently exists.');
			this.outputModelDirectories();
		} else {
			console.log('    Pre-existing directory found.');
			if (fs.existsSync(this.targetDirectory + '/simulation.xml')) {
				console.log('    Appears to be previous configuration. Attempting removal...');
				fs.rmdirRecursive(this.targetDirectory);
				this.outputModelDirectories();
			} else {
				console.log('    Does not appear to be previous configuration. Aborting...');
			}
		}
	});
};

Model.prototype.outputModelDirectories = function() {
	console.log('    Creating directory structure...');
	fs.mkdirRecursive(this.targetDirectory, null, () => {
		fs.mkdirSync(this.targetDirectory + '/topography');
		fs.mkdirSync(this.targetDirectory + '/boundaries');
		fs.mkdirSync(this.targetDirectory + '/output');
		console.log('    Required directories created.');
		this.outputReady = true;
		this.outputModelFiles();
	});
};

Model.prototype.outputModelFiles = function() {
	console.log('    Creating output files...');
	
	this.outputModelTopography();
	this.outputModelConfiguration();
	this.outputModelBoundaries();
};

Model.prototype.outputModelTopography = function() {
	var sourceFile = this.domain.getPathTopography();
	console.log('    Attempting to copy topography files.');
	
	if (!sourceFile) {
		console.log('    No topography file available.');
		return false;
	}
	
	fs.copyFile(
		sourceFile,
		this.targetDirectory + '/topography/MODEL_TOPOGRAPHY.img',
		(err) => {
			if (!err) {
				console.log('    Topography file written.');
				this.outputTopography = true;
			} else {
				console.log('    An error occured writing the topography file.');
			}
		}
	);
}

Model.prototype.outputModelBoundaries = function() {
	console.log('    Attempting to output boundary files.');
	this.boundaries.writeFiles(
		this.duration,
		this.targetDirectory + '/boundaries/',
		(err) => {
			if (!err) {
				console.log('    Boundary files written.');
				this.outputBoundaries = true;
			} else {
				console.log('    An error occured writing the boundary files.');
			}
		}
	)
}

Model.prototype.outputModelConfiguration = function() {
	console.log('    Attempting to write configuration file.');
	fs.writeFile(
		this.targetDirectory + '/simulation.xml',
		this.getXMLFile(),
		(err) => {
			if (!err) {
				console.log('    Configuration file written.');
				this.outputConfiguration = true;
			} else {
				console.log('    An error occured writing the configuration file.');
			}
		}
	);
}

Model.prototype.getXMLFile = function() {
	let xml = '\
	<?xml version="1.0"?>\n\
	<!DOCTYPE configuration PUBLIC "HiPIMS Configuration Schema 1.1" "http://www.lukesmith.org.uk/research/namespace/hipims/1.1/"[]>\n\
	<configuration>\n\
		<metadata>\n\
			<name>' + this.name + '</name>\n\
			<description>Automatically built ' + this.source + ' model.</description>\n\
		</metadata>\n\
		<execution>\n\
			<executor name="OpenCL">\n\
				<parameter name="deviceFilter" value="GPU,CPU" />\n\
			</executor>\n\
		</execution>\n\
		<simulation>\n\
			<parameter name="duration" value="' + this.duration + '" />\n\
			<parameter name="outputFrequency" value="' + this.outputFrequency + '" />\n\
			<parameter name="floatingPointPrecision" value="double" />\n\
			<domainSet>\n\
				<domain type="cartesian" deviceNumber="1">\n\
					<data sourceDir="topography/"\n\
						  targetDir="output/">\n\
						<dataSource type="raster" value="structure,dem" source="MODEL_TOPOGRAPHY.img" />\n\
						<dataSource type="constant" value="manningCoefficient" source="0.020" />\n\
						<dataSource type="constant" value="depth" source="0.0" />\n\
						<dataSource type="constant" value="velocityX" source="0.0" />\n\
						<dataSource type="constant" value="velocityY" source="0.0" />\n\
						<dataTarget type="raster" value="depth" format="HFA" target="depth_dem_%t.img" />\n\
						<dataTarget type="raster" value="velocityX" format="HFA" target="velX_dem_%t.img" />\n\
						<dataTarget type="raster" value="velocityY" format="HFA" target="velY_dem_%t.img" />\n\
						<dataTarget type="raster" value="fsl" format="HFA" target="fsl_dem_%t.img" />\n\
						<dataTarget type="raster" value="maxdepth" format="HFA" target="maxdepth_dem_%t.img" />\n\
					</data>\n\
					<scheme name="Godunov">\n\
						<parameter name="courantNumber" value="0.50" />\n\
						<parameter name="groupSize" value="32x8" />\n\
					</scheme>\n\
					<boundaryConditions sourceDir="boundaries/">\n\
						<timeseries type="atmospheric" name="Rainfall" value="rain-intensity" source="rainfall.csv" />\n\
						<timeseries type="atmospheric" name="Drainage" value="loss-rate" source="drainage.csv" />\n\
					</boundaryConditions>\n\
				</domain>\n\
			</domainSet>\n\
		</simulation>\n\
	</configuration>\n\
	';
	return xml;
}

// Avoid having to use another module for this
// H/T: http://lmws.net/making-directory-along-with-missing-parents-in-node-js
fs.mkdirRecursive = function(dirPath, mode, callback) {
	fs.mkdir(dirPath, mode, function(error) {
		if (error && error.code === 'ENOENT') {
			fs.mkdirRecursive(path.dirname(dirPath), mode);
			fs.mkdirRecursive(dirPath, mode, callback);
		} else {
			callback && callback(error);
		}
	});
};

fs.rmdirRecursive = function(dirPath) {
	// Protect against doing bad things...
	if (dirPath.length <= 1) {
		console.log('    Aborting removing directory. Sure the path is right?');
		return;
	}
	if (fs.existsSync(dirPath)) {
		fs.readdirSync(dirPath).forEach((file,index) => {
			let curPath = dirPath + "/" + file;
			if (fs.lstatSync(curPath).isDirectory()) {
				fs.rmdirRecursive(curPath);
			} else {
				fs.unlinkSync(curPath);
			}
		});
		fs.rmdirSync(dirPath);
	}
};

fs.copyFile = function(source, target, cb) {
  var cbCalled = false;

  var rd = fs.createReadStream(source);
  rd.on("error", function(err) {
    done(err);
  });
  var wr = fs.createWriteStream(target);
  wr.on("error", function(err) {
    done(err);
  });
  wr.on("close", function(ex) {
    done();
  });
  rd.pipe(wr);

  function done(err) {
    if (!cbCalled) { cb(err); cbCalled = true; }
  }
};

module.exports = Model;
