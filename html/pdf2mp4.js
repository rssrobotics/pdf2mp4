// doc is a hashtable:
//  slides => array of "slide"
//  display_idx => 4   (which slide is being shown?)
//
// slide is a hashtable:
//   thumb => "pdfthumb-1.png"
//   seconds => 25

// thumbnails are scaled to 320 pixels high (preserving aspect ratio)
var doc = new Object();

function xmlhttp_post(url, data, callback)
{
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange =
        function() {
            if (xmlhttp.readyState==4) {
                if (xmlhttp.status==200)
                    callback(xmlhttp, url);
                else
                    alert("XMLHTTP request to "+url+" got status "+xmlhttp.status+". Cross-domain request?");
            }
        };

    xmlhttp.open("POST", url, true);
    xmlhttp.setRequestHeader("Content-type", "application/x-www-form-urlencoded");

    // this header must be set by the browser. the standard says a browser should
    // terminate a request if Content-length or Connection are specified.
    //    xmlhttp.setRequestHeader("Content-length", data.length);

    xmlhttp.send(data);
}

function doc_create(nslides)
{
    doc = new Object();
    doc.slides = [];

    for (i = 0; i < nslides; i++) {
        var slide = new Object();
        slide['type'] = "slide";
        slide['thumb'] = "pdfthumb-"+(i+1)+".png";
        slide['seconds'] = Math.floor(5*60 / nslides);

        doc.slides.push(slide);
    }

    doc.display_idx = 0;

    doc_rebuild_gui();
    doc_save();
}

// deserialize a saved document
function doc_restore(json_string)
{
    doc = JSON.parse(json_string);

    doc_rebuild_gui();
    doc_save();
}

var doc_save_triggered = 0;
var doc_save_in_progress = 0;

window.setInterval(function () {
    if (doc_save_triggered && !doc_save_in_progress) {
        doc_save_triggered = 0;
        doc_save_real();
        }
}, 2000);

// request a save in the near future
function doc_save()
{
    var status_label = document.getElementById("status_label");
    if (status_label)
        status_label.innerHTML="<font color='#ff0000'>Save pending...</font><br>";

    doc_save_triggered = 1;
}

// serialize the document and push to the server
function doc_save_real()
{
    doc_save_in_progress = 1;

    var status_label = document.getElementById("status_label");
    if (status_label)
        status_label.innerHTML="Saving...";

    var json = JSON.stringify(doc);

    xmlhttp_post("project_write.php", "key="+doc.key+"&data="+json,
                  function(xml, url) {
                      try {
                          var resp = JSON.parse(xml.responseText);
                          if (resp != "OK") {
                              alert("Error saving document: "+resp);
                              if (status_label)
                                  status_label.innerHTML="Last save failed.";
                          } else {
                              if (status_label)
                                  status_label.innerHTML="Saved<br>"+(new Date()).toTimeString();
                          }
                      } catch (ex) {
                              if (status_label)
                                  status_label.innerHTML="Last save failed.";

                          alert("Error saving document: couldn't parse response "+xml.responseText);
                      } finally {
                          doc_save_in_progress = 0;
                      }
                  });
}

// display a different slide in the thumbnail area, i.e., call this after setting doc.display_idx
function doc_set_display_idx(idx)
{
    document.getElementById("slide_nav_"+doc.display_idx).className="thumb";

    idx = Math.min(doc.slides.length-1, idx);
    idx = Math.max(0, idx);
    doc.display_idx = idx;

    var image = document.getElementById("current_slide");
    image.src = project_url+"/"+doc.slides[doc.display_idx].thumb;
    document.getElementById("slide_seconds").value = doc.slides[doc.display_idx].seconds;

    var progress = document.getElementById("slide_progress_bar");
    progress.checked = doc.slides[doc.display_idx].progress;

    document.getElementById("slide_nav_"+idx).className="thumb_selected";
}

function doc_recompute_total_seconds()
{
    var seconds = 0;
    for (var i = 0; i < doc.slides.length; i++) {
        var slide = doc.slides[i];
        seconds += slide.seconds;
    }

    document.getElementById("total_seconds").innerHTML = ""+seconds;
}

// called when the number of slides has changes, or other
// large-scale changes have occurred.
function doc_rebuild_gui()
{
    if (1) {
        var slide_nav = document.getElementById("slide_nav");

        slide_nav.innerHTML = "";

        for (var i = 0; i < doc.slides.length; i++) {
            var slide = doc.slides[i];

            slide_nav.innerHTML += "<img id=slide_nav_"+i+" width="+(slide_nav.offsetWidth-40)+" class=thumb onclick='doc_set_display_idx("+i+")' src="+project_url+"/"+ slide.thumb + "><br>";
        }
    }

    // don't require them to hit enter to get live updates
    document.getElementById("slide_seconds").onkeyup = function() {
        var f = parseFloat(document.getElementById("slide_seconds").value);
        if (isFinite(f)) {
            doc.slides[doc.display_idx].seconds = Math.max(0, f);
            doc_recompute_total_seconds();
        }
        doc_save();
    };

    // display validated results once they hit enter
    document.getElementById("slide_seconds").onchange = function() {
        document.getElementById("slide_seconds").value = doc.slides[doc.display_idx].seconds;
        doc_recompute_total_seconds();
        doc_save();
    };

    document.getElementById("current_slide_label").innerHTML = " "+(doc.display_idx+1)+" / "+doc.slides.length+" ";
    document.getElementById("next_button").onclick = function() {
        doc_set_display_idx(doc.display_idx + 1);
    }

    document.getElementById("prev_button").onclick = function() {
        doc_set_display_idx(doc.display_idx - 1);
    }

    document.getElementById("project_name").value = doc.name;
    document.getElementById("project_name").onkeyup = function() {
        doc.name = document.getElementById("project_name").value;
        doc_save();
        doc_register_in_local_storage();
    }

    document.getElementById("slide_progress_bar").onclick = function() {
        doc.slides[doc.display_idx].progress = document.getElementById("slide_progress_bar").checked;
        doc_save();
    }

    document.getElementById("global_progress_bar").checked = doc.progress;
    document.getElementById("global_progress_bar").onclick = function() {
        doc.progress = document.getElementById("global_progress_bar").checked ? 1 : 0;
        doc_save();
    }

    doc_set_display_idx(doc.display_idx);
    doc_recompute_total_seconds();

    doc_register_in_local_storage();
}

function doc_register_in_local_storage()
{
    // make sure localStorage knows about this project
    var projects = JSON.parse(localStorage.getItem("projects"));
    if (projects == null) {
        projects = new Object();
    }

    projects[doc.key] = doc;
    localStorage["projects"] = JSON.stringify(projects);
}
