
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
   loadPackage({"files": [{"filename": "/resources/img/background-1130x840.png", "start": 0, "end": 404627, "audio": 0}, {"filename": "/resources/img/btnPatF.png", "start": 404627, "end": 412058, "audio": 0}, {"filename": "/resources/img/btnPatD#.png", "start": 412058, "end": 419845, "audio": 0}, {"filename": "/resources/img/btnPatE.png", "start": 419845, "end": 425484, "audio": 0}, {"filename": "/resources/img/btnInternalSync.png", "start": 425484, "end": 437941, "audio": 0}, {"filename": "/resources/img/606-2.png", "start": 437941, "end": 1690572, "audio": 0}, {"filename": "/resources/img/o303.png", "start": 1690572, "end": 2409767, "audio": 0}, {"filename": "/resources/img/btnPatMod.knob", "start": 2409767, "end": 4785450, "audio": 0}, {"filename": "/resources/img/btnPatB.png", "start": 4785450, "end": 4793007, "audio": 0}, {"filename": "/resources/img/btnMidi.png", "start": 4793007, "end": 4805454, "audio": 0}, {"filename": "/resources/img/btnProp-Turkos.png", "start": 4805454, "end": 4807441, "audio": 0}, {"filename": "/resources/img/btnC.png", "start": 4807441, "end": 4810346, "audio": 0}, {"filename": "/resources/img/btnKeySync.png", "start": 4810346, "end": 4823031, "audio": 0}, {"filename": "/resources/img/btnStop.png", "start": 4823031, "end": 4834037, "audio": 0}, {"filename": "/resources/img/606-2.knob", "start": 4834037, "end": 4922505, "audio": 0}, {"filename": "/resources/img/fx-1-100x100.png", "start": 4922505, "end": 5919100, "audio": 0}, {"filename": "/resources/img/ola-background-1130x840.xcf", "start": 5919100, "end": 14176693, "audio": 0}, {"filename": "/resources/img/background-1130x840.xcf", "start": 14176693, "end": 15226506, "audio": 0}, {"filename": "/resources/img/btnLed.png", "start": 15226506, "end": 15231185, "audio": 0}, {"filename": "/resources/img/btnC.knob", "start": 15231185, "end": 17612307, "audio": 0}, {"filename": "/resources/img/knobLoopSize.knob", "start": 17612307, "end": 17704805, "audio": 0}, {"filename": "/resources/img/btnAccent.bmp", "start": 17704805, "end": 17729435, "audio": 0}, {"filename": "/resources/img/btnPatC.png", "start": 17729435, "end": 17737034, "audio": 0}, {"filename": "/resources/img/btnEffects.png", "start": 17737034, "end": 17745121, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.png", "start": 17745121, "end": 18851279, "audio": 0}, {"filename": "/resources/img/btnWaveForm.png", "start": 18851279, "end": 18858128, "audio": 0}, {"filename": "/resources/img/btnEffects.xcf", "start": 18858128, "end": 18870230, "audio": 0}, {"filename": "/resources/img/btnProp.png", "start": 18870230, "end": 18872217, "audio": 0}, {"filename": "/resources/img/btnPatD.png", "start": 18872217, "end": 18879741, "audio": 0}, {"filename": "/resources/img/btnPatternOctav2.png", "start": 18879741, "end": 18892223, "audio": 0}, {"filename": "/resources/img/btnHostSync.png", "start": 18892223, "end": 18905086, "audio": 0}, {"filename": "/resources/img/btnPatternOctav3.png", "start": 18905086, "end": 18917861, "audio": 0}, {"filename": "/resources/img/btnPatG#.png", "start": 18917861, "end": 18925743, "audio": 0}, {"filename": "/resources/img/btnSeq.png", "start": 18925743, "end": 18927776, "audio": 0}, {"filename": "/resources/img/btnAccent.knob", "start": 18927776, "end": 18968652, "audio": 0}, {"filename": "/resources/img/fx-1-175x175.knob", "start": 18968652, "end": 19067074, "audio": 0}, {"filename": "/resources/img/btnPatA.png", "start": 19067074, "end": 19074675, "audio": 0}, {"filename": "/resources/img/btnPtnCopy.png", "start": 19074675, "end": 19085770, "audio": 0}, {"filename": "/resources/img/fx1-1.png", "start": 19085770, "end": 21399420, "audio": 0}, {"filename": "/resources/img/btnPatC#.png", "start": 21399420, "end": 21407283, "audio": 0}, {"filename": "/resources/img/btnHostSync.knob", "start": 21407283, "end": 23782438, "audio": 0}, {"filename": "/resources/img/btnPatA#.png", "start": 23782438, "end": 23790305, "audio": 0}, {"filename": "/resources/img/btnLed.knob", "start": 23790305, "end": 23831428, "audio": 0}, {"filename": "/resources/img/btnPatG.png", "start": 23831428, "end": 23839055, "audio": 0}, {"filename": "/resources/img/knobLoopSize.png", "start": 23839055, "end": 23959917, "audio": 0}, {"filename": "/resources/img/btnSeq.knob", "start": 23959917, "end": 26346324, "audio": 0}, {"filename": "/resources/img/o303-1130x840.png", "start": 26346324, "end": 27022886, "audio": 0}, {"filename": "/resources/img/btnPatternOctav.knob", "start": 27022886, "end": 29404539, "audio": 0}, {"filename": "/resources/img/btnProp-Brons.png", "start": 29404539, "end": 29406611, "audio": 0}, {"filename": "/resources/img/btnPatF#.png", "start": 29406611, "end": 29414341, "audio": 0}, {"filename": "/resources/img/btnPtnClear.png", "start": 29414341, "end": 29426122, "audio": 0}, {"filename": "/resources/img/btnProp-Blue.png", "start": 29426122, "end": 29428118, "audio": 0}, {"filename": "/resources/img/606-2-ola-1.png", "start": 29428118, "end": 29441012, "audio": 0}, {"filename": "/resources/img/btnPtnRand.png", "start": 29441012, "end": 29453546, "audio": 0}], "remote_package_size": 29453546, "package_uuid": "7ea21835-952f-4308-a185-5db790272c5d"});
  
  })();
  
