/*******************************************************************************
  Helper functions for spn_corpus_view
*******************************************************************************/

var D3Loaded = false;
//var vizJsLoaded = false;

function renderMatrix(node, obj) {
    var infoNode = document.createElement('div');
    node.appendChild(infoNode);
    var row = obj['data'].length;
    var col = obj['data'][0].length;
    infoNode.innerHTML = "<div><b>Number of rows: </b> " + row + "</div><div><b>Number of cols: </b> " + col + "</div><div id=\"heatmap\"></div>";
    if (row > 1 && col > 1) {
        plotlyData = [
            {
                z: obj['data'],
                type: 'heatmap'
            }
        ]
        Plotly.newPlot('heatmap', plotlyData)
    }
}

function renderFST(node, obj) {
    var infoNode = document.createElement('div');
    node.appendChild(infoNode);
    var graphNode = document.createElement('div');
    infoNode.appendChild(graphNode);
    // FST drawing is temporary disabled.
    /*
    var withVizJs = function() {
        vizJsLoaded = true;
        var lines = obj.src.split('\n');
        var usedStates = new Set()
        var finalStates = {};
        var edges = [];
        var nodes = [];
        for (var linum = 0; linum < lines.length; ++ linum) {
            var elems = lines[linum].split('\t');
            if (lines[linum].length == 0) continue;
            if (elems.length == 2) {
                var st = parseInt(elems[0]);
                finalStates[st] = elems[1];
                usedStates.add(st);
            } else {
                var from = parseInt(elems[0]), to = parseInt(elems[1]);
                var label = elems[2] + ":" + elems[3] + "\n/" + elems[4];
                edges.push({from: from, to: to, label: label})
                usedStates.add(from);
                usedStates.add(to);

            }
        }
        var stArray = Array.from(usedStates);
        stArray.sort();
        for (var n = 0; n < stArray.length; ++ n) {
            var st = stArray[n];
            var label = st.toString();
            if (st in finalStates) {
                label += "\n/" + finalStates[st];
            }
            nodes.push({id: st, label: label});
        }

        new vis.Network(graphNode, {
            nodes: new vis.DataSet(nodes),
            edges: new vis.DataSet(edges)
        }, {
            layout: {
                hierarchical: {
                    direction: "LR",
                    sortMethod: "directed"
                }
            }
        });;
    }

    if (! vizJsLoaded) {
        loadExternalCSS('https://cdnjs.cloudflare.com/ajax/libs/vis/4.14.0/vis.min.css')
        loadExternalScript('https://cdnjs.cloudflare.com/ajax/libs/vis/4.14.0/vis.min.js', withVizJs)
    } else {
        withVizJs();
    }
    */
}

function renderItem(node, obj) {
    for (var propName in obj) {
        var value = obj[propName];
        var fieldNode = document.createElement('div')
        node.appendChild(fieldNode);
        if (typeof(value) != "object") {
            fieldNode.innerHTML = "<b>" + propName + "</b>: " + value + "";
        } else {
            if (value["type"] == undefined) {
                console.log("type for prop " + propName + " is undefined");
                continue;
            }
            else if (value["type"] == "matrix") {
                renderMatrix(fieldNode, value);
            } else if (value["type"] == "fst") {
                renderFST(fieldNode, value);
            } else {
                console.log("type for prop " + propName + " is unknown (" + value["type"] + ")");
            }
            node.appendChild(fieldNode);
        }
    }
}

function loadExternalScript(uri, callback) {
    var s = document.createElement('script');
    s.type = 'text/javascript';
    s.async = true;
    s.src = uri;
    var x = document.getElementsByTagName('script')[0];
    if (callback) {
        s.addEventListener('load', function (e) { callback(null, e); }, false);
    }
    x.parentNode.insertBefore(s, x);
}

function loadExternalCSS(uri, callback) {
    var s = document.createElement('link');
    s.rel = "stylesheet";
    s.type = "text/css";
    s.href = uri;
    var x = document.getElementsByTagName('link')[0];
    x.parentNode.insertBefore(s, x);
}

window.onload = function() {
    // load D3.js
    //loadExternalScript('http://d3js.org/d3.v3.min.js');
    // load plotly
    loadExternalScript('https://cdn.plot.ly/plotly-latest.min.js', function() {
        D3Loaded = true;
        var rootNode = document.createElement('div');
        document.body.appendChild(rootNode);
        renderItem(rootNode, item_data);
    })

}
