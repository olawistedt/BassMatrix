
  var Module = typeof Module !== 'undefined' ? Module : {};
  
  if (!Module.expectedDataFileDownloads) {
    Module.expectedDataFileDownloads = 0;
  }
  Module.expectedDataFileDownloads++;
  (function() {
   var loadPackage = function(metadata) {
  
      var PACKAGE_PATH;
      if (typeof window === 'object') {
        PACKAGE_PATH = window['encodeURIComponent'](window.location.pathname.toString().substring(0, window.location.pathname.toString().lastIndexOf('/')) + '/');
      } else if (typeof location !== 'undefined') {
        // worker
        PACKAGE_PATH = encodeURIComponent(location.pathname.toString().substring(0, location.pathname.toString().lastIndexOf('/')) + '/');
      } else {
        throw 'using preloaded data can only be done on a web page or in a web worker';
      }
      var PACKAGE_NAME = 'imgs.data';
      var REMOTE_PACKAGE_BASE = 'imgs.data';
      if (typeof Module['locateFilePackage'] === 'function' && !Module['locateFile']) {
        Module['locateFile'] = Module['locateFilePackage'];
        err('warning: you defined Module.locateFilePackage, that has been renamed to Module.locateFile (using your locateFilePackage for now)');
      }
      var REMOTE_PACKAGE_NAME = Module['locateFile'] ? Module['locateFile'](REMOTE_PACKAGE_BASE, '') : REMOTE_PACKAGE_BASE;
    
      var REMOTE_PACKAGE_SIZE = metadata['remote_package_size'];
      var PACKAGE_UUID = metadata['package_uuid'];
    
      function fetchRemotePackage(packageName, packageSize, callback, errback) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', packageName, true);
        xhr.responseType = 'arraybuffer';
        xhr.onprogress = function(event) {
          var url = packageName;
          var size = packageSize;
          if (event.total) size = event.total;
          if (event.loaded) {
            if (!xhr.addedTotal) {
              xhr.addedTotal = true;
              if (!Module.dataFileDownloads) Module.dataFileDownloads = {};
              Module.dataFileDownloads[url] = {
                loaded: event.loaded,
                total: size
              };
            } else {
              Module.dataFileDownloads[url].loaded = event.loaded;
            }
            var total = 0;
            var loaded = 0;
            var num = 0;
            for (var download in Module.dataFileDownloads) {
            var data = Module.dataFileDownloads[download];
              total += data.total;
              loaded += data.loaded;
              num++;
            }
            total = Math.ceil(total * Module.expectedDataFileDownloads/num);
            if (Module['setStatus']) Module['setStatus']('Downloading data... (' + loaded + '/' + total + ')');
          } else if (!Module.dataFileDownloads) {
            if (Module['setStatus']) Module['setStatus']('Downloading data...');
          }
        };
        xhr.onerror = function(event) {
          throw new Error("NetworkError for: " + packageName);
        }
        xhr.onload = function(event) {
          if (xhr.status == 200 || xhr.status == 304 || xhr.status == 206 || (xhr.status == 0 && xhr.response)) { // file URLs can return 0
            var packageData = xhr.response;
            callback(packageData);
          } else {
            throw new Error(xhr.statusText + " : " + xhr.responseURL);
          }
        };
        xhr.send(null);
      };

      function handleError(error) {
        console.error('package error:', error);
      };
    
    function runWithFS() {
  
      function assert(check, msg) {
        if (!check) throw msg + new Error().stack;
      }
  Module['FS_createPath']("/", "resources", true, true);
Module['FS_createPath']("/resources", "img", true, true);

          /** @constructor */
          function DataRequest(start, end, audio) {
            this.start = start;
            this.end = end;
            this.audio = audio;
          }
          DataRequest.prototype = {
            requests: {},
            open: function(mode, name) {
              this.name = name;
              this.requests[name] = this;
              Module['addRunDependency']('fp ' + this.name);
            },
            send: function() {},
            onload: function() {
              var byteArray = this.byteArray.subarray(this.start, this.end);
              this.finish(byteArray);
            },
            finish: function(byteArray) {
              var that = this;
      
          Module['FS_createPreloadedFile'](this.name, null, byteArray, true, true, function() {
            Module['removeRunDependency']('fp ' + that.name);
          }, function() {
            if (that.audio) {
              Module['removeRunDependency']('fp ' + that.name); // workaround for chromium bug 124926 (still no audio with this, but at least we don't hang)
            } else {
              err('Preloading file ' + that.name + ' failed');
            }
          }, false, true); // canOwn this data in the filesystem, it is a slide into the heap that will never change
  
              this.requests[this.name] = null;
            }
          };
      
              var files = metadata['files'];
              for (var i = 0; i < files.length; ++i) {
                new DataRequest(files[i]['start'], files[i]['end'], files[i]['audio']).open('GET', files[i]['filename']);
              }
      
        
        var indexedDB;
        if (typeof window === 'object') {
          indexedDB = window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
        } else if (typeof location !== 'undefined') {
          // worker
          indexedDB = self.indexedDB;
        } else {
          throw 'using IndexedDB to cache data can only be done on a web page or in a web worker';
        }
        var IDB_RO = "readonly";
        var IDB_RW = "readwrite";
        var DB_NAME = "/";
        var DB_VERSION = 1;
        var METADATA_STORE_NAME = 'METADATA';
        var PACKAGE_STORE_NAME = 'PACKAGES';
        function openDatabase(callback, errback) {
          try {
            var openRequest = indexedDB.open(DB_NAME, DB_VERSION);
          } catch (e) {
            return errback(e);
          }
          openRequest.onupgradeneeded = function(event) {
            var db = event.target.result;

            if(db.objectStoreNames.contains(PACKAGE_STORE_NAME)) {
              db.deleteObjectStore(PACKAGE_STORE_NAME);
            }
            var packages = db.createObjectStore(PACKAGE_STORE_NAME);

            if(db.objectStoreNames.contains(METADATA_STORE_NAME)) {
              db.deleteObjectStore(METADATA_STORE_NAME);
            }
            var metadata = db.createObjectStore(METADATA_STORE_NAME);
          };
          openRequest.onsuccess = function(event) {
            var db = event.target.result;
            callback(db);
          };
          openRequest.onerror = function(error) {
            errback(error);
          };
        };

        // This is needed as chromium has a limit on per-entry files in IndexedDB
        // https://cs.chromium.org/chromium/src/content/renderer/indexed_db/webidbdatabase_impl.cc?type=cs&sq=package:chromium&g=0&l=177
        // https://cs.chromium.org/chromium/src/out/Debug/gen/third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h?type=cs&sq=package:chromium&g=0&l=60
        // We set the chunk size to 64MB to stay well-below the limit
        var CHUNK_SIZE = 64 * 1024 * 1024;

        function cacheRemotePackage(
          db,
          packageName,
          packageData,
          packageMeta,
          callback,
          errback
        ) {
          var transactionPackages = db.transaction([PACKAGE_STORE_NAME], IDB_RW);
          var packages = transactionPackages.objectStore(PACKAGE_STORE_NAME);
          var chunkSliceStart = 0;
          var nextChunkSliceStart = 0;
          var chunkCount = Math.ceil(packageData.byteLength / CHUNK_SIZE);
          var finishedChunks = 0;
          for (var chunkId = 0; chunkId < chunkCount; chunkId++) {
            nextChunkSliceStart += CHUNK_SIZE;
            var putPackageRequest = packages.put(
              packageData.slice(chunkSliceStart, nextChunkSliceStart),
              'package/' + packageName + '/' + chunkId
            );
            chunkSliceStart = nextChunkSliceStart;
            putPackageRequest.onsuccess = function(event) {
              finishedChunks++;
              if (finishedChunks == chunkCount) {
                var transaction_metadata = db.transaction(
                  [METADATA_STORE_NAME],
                  IDB_RW
                );
                var metadata = transaction_metadata.objectStore(METADATA_STORE_NAME);
                var putMetadataRequest = metadata.put(
                  {
                    'uuid': packageMeta.uuid,
                    'chunkCount': chunkCount
                  },
                  'metadata/' + packageName
                );
                putMetadataRequest.onsuccess = function(event) {
                  callback(packageData);
                };
                putMetadataRequest.onerror = function(error) {
                  errback(error);
                };
              }
            };
            putPackageRequest.onerror = function(error) {
              errback(error);
            };
          }
        }

        /* Check if there's a cached package, and if so whether it's the latest available */
        function checkCachedPackage(db, packageName, callback, errback) {
          var transaction = db.transaction([METADATA_STORE_NAME], IDB_RO);
          var metadata = transaction.objectStore(METADATA_STORE_NAME);
          var getRequest = metadata.get('metadata/' + packageName);
          getRequest.onsuccess = function(event) {
            var result = event.target.result;
            if (!result) {
              return callback(false, null);
            } else {
              return callback(PACKAGE_UUID === result['uuid'], result);
            }
          };
          getRequest.onerror = function(error) {
            errback(error);
          };
        }

        function fetchCachedPackage(db, packageName, metadata, callback, errback) {
          var transaction = db.transaction([PACKAGE_STORE_NAME], IDB_RO);
          var packages = transaction.objectStore(PACKAGE_STORE_NAME);

          var chunksDone = 0;
          var totalSize = 0;
          var chunkCount = metadata['chunkCount'];
          var chunks = new Array(chunkCount);

          for (var chunkId = 0; chunkId < chunkCount; chunkId++) {
            var getRequest = packages.get('package/' + packageName + '/' + chunkId);
            getRequest.onsuccess = function(event) {
              // If there's only 1 chunk, there's nothing to concatenate it with so we can just return it now
              if (chunkCount == 1) {
                callback(event.target.result);
              } else {
                chunksDone++;
                totalSize += event.target.result.byteLength;
                chunks.push(event.target.result);
                if (chunksDone == chunkCount) {
                  if (chunksDone == 1) {
                    callback(event.target.result);
                  } else {
                    var tempTyped = new Uint8Array(totalSize);
                    var byteOffset = 0;
                    for (var chunkId in chunks) {
                      var buffer = chunks[chunkId];
                      tempTyped.set(new Uint8Array(buffer), byteOffset);
                      byteOffset += buffer.byteLength;
                      buffer = undefined;
                    }
                    chunks = undefined;
                    callback(tempTyped.buffer);
                    tempTyped = undefined;
                  }
                }
              }
            };
            getRequest.onerror = function(error) {
              errback(error);
            };
          }
        }
      
      function processPackageData(arrayBuffer) {
        assert(arrayBuffer, 'Loading data file failed.');
        assert(arrayBuffer instanceof ArrayBuffer, 'bad input to processPackageData');
        var byteArray = new Uint8Array(arrayBuffer);
        var curr;
        
          // Reuse the bytearray from the XHR as the source for file reads.
          DataRequest.prototype.byteArray = byteArray;
    
            var files = metadata['files'];
            for (var i = 0; i < files.length; ++i) {
              DataRequest.prototype.requests[files[i].filename].onload();
            }
                Module['removeRunDependency']('datafile_imgs.data');

      };
      Module['addRunDependency']('datafile_imgs.data');
    
      if (!Module.preloadResults) Module.preloadResults = {};
    
        function preloadFallback(error) {
          console.error(error);
          console.error('falling back to default preload behavior');
          fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE, processPackageData, handleError);
        };

        openDatabase(
          function(db) {
            checkCachedPackage(db, PACKAGE_PATH + PACKAGE_NAME,
              function(useCached, metadata) {
                Module.preloadResults[PACKAGE_NAME] = {fromCache: useCached};
                if (useCached) {
                  fetchCachedPackage(db, PACKAGE_PATH + PACKAGE_NAME, metadata, processPackageData, preloadFallback);
                } else {
                  fetchRemotePackage(REMOTE_PACKAGE_NAME, REMOTE_PACKAGE_SIZE,
                    function(packageData) {
                      cacheRemotePackage(db, PACKAGE_PATH + PACKAGE_NAME, packageData, {uuid:PACKAGE_UUID}, processPackageData,
                        function(error) {
                          console.error(error);
                          processPackageData(packageData);
                        });
                    }
                  , preloadFallback);
                }
              }
            , preloadFallback);
          }
        , preloadFallback);

        if (Module['setStatus']) Module['setStatus']('Downloading...');
      
    }
    if (Module['calledRun']) {
      runWithFS();
    } else {
      if (!Module['preRun']) Module['preRun'] = [];
      Module["preRun"].push(runWithFS); // FS is not initialized yet, wait for it
    }
  
   }
   loadPackage({"files": [{"filename": "/resources/img/fx1-1.png", "start": 0, "end": 2313650, "audio": 0}, {"filename": "/resources/img/btnStop.png", "start": 2313650, "end": 2318040, "audio": 0}, {"filename": "/resources/img/606-2.knob", "start": 2318040, "end": 2406508, "audio": 0}, {"filename": "/resources/img/btnPtnClear.png", "start": 2406508, "end": 2409842, "audio": 0}, {"filename": "/resources/img/btnLed.knob", "start": 2409842, "end": 2450965, "audio": 0}, {"filename": "/resources/img/fx-1-100x100.png", "start": 2450965, "end": 3447560, "audio": 0}, {"filename": "/resources/img/btnPatE.png", "start": 3447560, "end": 3449132, "audio": 0}, {"filename": "/resources/img/btnPatF#.png", "start": 3449132, "end": 3450895, "audio": 0}, {"filename": "/resources/img/btnMidi.png", "start": 3450895, "end": 3455586, "audio": 0}, {"filename": "/resources/img/btnPatMod.knob", "start": 3455586, "end": 5830526, "audio": 0}, {"filename": "/resources/img/btnSeq.png", "start": 5830526, "end": 5831798, "audio": 0}, {"filename": "/resources/img/btnPatF.png", "start": 5831798, "end": 5833342, "audio": 0}, {"filename": "/resources/img/btnPatD#.png", "start": 5833342, "end": 5835165, "audio": 0}, {"filename": "/resources/img/606-2.png", "start": 5835165, "end": 7087796, "audio": 0}, {"filename": "/resources/img/btnPatC.png", "start": 7087796, "end": 7089368, "audio": 0}, {"filename": "/resources/img/btnHostSync.knob", "start": 7089368, "end": 9464530, "audio": 0}, {"filename": "/resources/img/btnPatG.png", "start": 9464530, "end": 9466157, "audio": 0}, {"filename": "/resources/img/btnPatB.png", "start": 9466157, "end": 9467737, "audio": 0}, {"filename": "/resources/img/btnAccent.knob", "start": 9467737, "end": 9508613, "audio": 0}, {"filename": "/resources/img/btnPatD.png", "start": 9508613, "end": 9510231, "audio": 0}, {"filename": "/resources/img/btnPatC#.png", "start": 9510231, "end": 9512055, "audio": 0}, {"filename": "/resources/img/606-2-ola-1.png", "start": 9512055, "end": 9524949, "audio": 0}, {"filename": "/resources/img/knobLoopSize.png", "start": 9524949, "end": 9584606, "audio": 0}, {"filename": "/resources/img/btnPatternOctav3.png", "start": 9584606, "end": 9587842, "audio": 0}, {"filename": "/resources/img/o303-1130x840.png", "start": 9587842, "end": 10264404, "audio": 0}, {"filename": "/resources/img/background-1130x840.png", "start": 10264404, "end": 10922504, "audio": 0}, {"filename": "/resources/img/btnPatA.png", "start": 10922504, "end": 10924099, "audio": 0}, {"filename": "/resources/img/btnHostSync.png", "start": 10924099, "end": 10929027, "audio": 0}, {"filename": "/resources/img/background-1130x840.xcf", "start": 10929027, "end": 19186620, "audio": 0}, {"filename": "/resources/img/btnC.knob", "start": 19186620, "end": 21567162, "audio": 0}, {"filename": "/resources/img/o303.png", "start": 21567162, "end": 22286357, "audio": 0}, {"filename": "/resources/img/btnPatA#.png", "start": 22286357, "end": 22288154, "audio": 0}, {"filename": "/resources/img/btnSeq.knob", "start": 22288154, "end": 24674561, "audio": 0}, {"filename": "/resources/img/btnPatternOctav2.png", "start": 24674561, "end": 24677783, "audio": 0}, {"filename": "/resources/img/btnKeySync.png", "start": 24677783, "end": 24682522, "audio": 0}, {"filename": "/resources/img/btnAccent.bmp", "start": 24682522, "end": 24707152, "audio": 0}, {"filename": "/resources/img/knobLoopSize.knob", "start": 24707152, "end": 24796510, "audio": 0}, {"filename": "/resources/img/btnC.png", "start": 24796510, "end": 24799415, "audio": 0}, {"filename": "/resources/img/btnPatG#.png", "start": 24799415, "end": 24801241, "audio": 0}, {"filename": "/resources/img/btnLed.png", "start": 24801241, "end": 24805920, "audio": 0}, {"filename": "/resources/img/btnPtnCopy.png", "start": 24805920, "end": 24809155, "audio": 0}, {"filename": "/resources/img/btnPatternOctav.knob", "start": 24809155, "end": 27190235, "audio": 0}, {"filename": "/resources/img/btnInternalSync.png", "start": 27190235, "end": 27194943, "audio": 0}, {"filename": "/resources/img/btnPtnRand.png", "start": 27194943, "end": 27198700, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.knob", "start": 27198700, "end": 27297122, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.png", "start": 27297122, "end": 28403280, "audio": 0}], "remote_package_size": 28403280, "package_uuid": "98ef4496-637c-4de0-8b1b-f0111816219b"});
  
  })();
  
