#!/usr/bin/env node
var path = require('path');
var fs = require('fs');
var mkdirp = require('mkdirp');
var child_process = require('child_process');
var sourcePath = path.join(__dirname, '/../../src');
var destinationPath = path.join(__dirname, '/../../build/js');

var namespace = 'machinetalk/protobuf';

// Create list of information for protobuf files.
var protos = fs.readdirSync(path.join(sourcePath, namespace))
  .filter(function(filename) {
    // Only handle .proto files.
    return /\.proto$/.test(filename);
  })
  .map(function(filename) {
    var name = path.basename(filename, '.proto');
    var fullname = path.join(namespace, name);
    return {
      filename: filename,
      name: name,
      fullname: fullname,
      protopath: path.join(sourcePath, namespace, filename),
      jspath: path.join(destinationPath, namespace, name + '.js')
    };
  });

// Generate js file for each proto file
protos.forEach(function(protos) {
  mkdirp.sync(path.dirname(protos.jspath));
  child_process.execFileSync('pbjs', ['--target', 'commonjs', '--out', protos.jspath, '--path', sourcePath, protos.protopath]);
});


// Generate json file to direct index.js to the generated js files.
var protosjson = protos.map(function(proto) {
  return {
    name: proto.name,
    fullname: proto.fullname
  };
});

mkdirp.sync(destinationPath);
var text = '';
protosjson.forEach(function(proto) {
    text += 'module.exports[\'' + proto.name + '\'] = require(\'./' +  proto.fullname + '.js\').pb;\n';
});
fs.writeFileSync(path.join(destinationPath, 'protoexport.js'), text);
