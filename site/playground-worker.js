/* Execute the actual compiler and VM away from the page's event loop. */
self.onmessage = async function (event) {
  var source = event.data.source;
  var output = [];
  var diagnostics = [];
  var outputSize = 0;
  function collect(destination, line) {
    outputSize += line.length;
    if (outputSize > 32768) throw new Error('Output limit reached. Try printing fewer lines.');
    destination.push(line);
  }
  try {
    if (typeof source !== 'string' || source.length > 65536) throw new Error('Keep the program under 64 KB.');
    self.postMessage({type: 'stage', stage: 'loading'});
    importScripts('assets/playground-runtime.js');
    var runtime = await createPlayground({
      locateFile: function (name) { return new URL('assets/' + name, self.location.href).href; },
      print: function (line) { collect(output, line); },
      printErr: function (line) { collect(diagnostics, line); }
    });
    runtime.FS.writeFile('/playground.zi', source);
    var result = runtime.ccall('RunSource', 'number', [], []);
    self.postMessage({type: 'result', ok: result === 0, output: output.join('\n'), diagnostics: diagnostics.join('\n')});
  } catch (error) {
    self.postMessage({type: 'result', ok: false, output: output.join('\n'), diagnostics: error.message || 'Unable to run this program.'});
  }
  self.close();
};
