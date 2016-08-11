<?php

include("common.php");

validate_key($key);

$fd = fopen($PROJECT_JSON, "r");
if (!$fd) {
    print "Failed to read json.";
  // new document
    die();
}

$json = fgets($fd);
fclose($fd);

$doc = json_decode($json, 1);

$t = time();

if ($_REQUEST["preview"]==1) {
    $fps = intval($doc["pfps"]);
    $h = intval($doc["pheight"]);
    $bitrate = intval($doc["pbitrate"]);
    $slideselector = "thumb";
    $taskbase = "preview";
    $pri = 0;
} else {
    $fps = intval($doc["fps"]);
    $h = intval($doc["height"]);
    $bitrate = intval($doc["bitrate"]);
    $slideselector = "slide";
    $taskbase = "final";
    $pri = 1;
}

$task = $taskbase."_$t";
$outputpath = $PROJECT_DIR."/$task.mp4";
$outputurl = $PROJECT_URL."/$task.mp4";
$projectscheme = $PROJECT_DIR."/project_$task.scheme";

chmod($PROJECT_DIR, 0777);
unlink($outputpath);

$w = intval(floatval($doc["aspect"]) * $h);
if (($w % 4) != 0)
    $w += 4 - ($w % 4);

/////////////////////////////////////////
// write the output
$fd = fopen($projectscheme, "w");

$totalseconds = 0;

fprintf($fd, "(define crossfade-time 0.75)\n\n");

for ($i = 0; $i < count($doc["slides"]); $i++) {
    $slide = $doc["slides"][$i];

    if ($slide["type"] == "slide") {
        $slidepath = $PROJECT_DIR."/".$slide[$slideselector];

        fprintf($fd, "(define slide_$i \n");
        fprintf($fd, "  (image-source-matte $w $h \"#000000\"\n");
        fprintf($fd, "    (image-source-create-from-image-cache (+ crossfade-time ".floatval($slide["seconds"]).") \"$slidepath\")))\n");

        if ($slide["progress"])
            fprintf($fd, "(set! slide_$i (image-source-progress-bar -1 4 \"".$doc["progress_rgb"]."\" \"#000000\" slide_$i))\n");

        fprintf($fd, "\n");
    } else if ($slide["type"] == "movie") {
        fprintf($fd, "(define slide_$i \n");
        fprintf($fd, "  (image-source-matte $w $h \"#000000\"\n");
        fprintf($fd, "    (image-source-scale -1 $h\n");
        fprintf($fd, "      (image-source-create-from-path ".$slide["seconds"]." ".$slide["playfps"]." '".$slide["mode"]." \"".$PROJECT_DIR."/".$slide["dir"]."/\"))))\n");

        if ($slide["progress"])
            fprintf($fd, "(set! slide_$i (image-source-progress-bar -1 4 \"".$doc["progress_rgb"]."\" \"#000000\" slide_$i))\n");

        fprintf($fd, "\n");
    } else {
        // hmm, not supported?
    }

    $totalseconds += $slide["seconds"];
}

fprintf($fd, "\n");

fprintf($fd, "(define vid (crossfade-all crossfade-time (list\n");

for ($i = 0; $i < count($doc["slides"]); $i++) {
    fprintf($fd, "                               slide_$i\n");
}

fprintf($fd, "       (image-source-create-from-image 2 (image-create $w $h \"#000000\")))))\n");
fprintf($fd, "\n");
fprintf($fd, "\n");
if ($doc["progress"])
    fprintf($fd, "(set! vid (image-source-progress-bar 0 4 \"".$doc["progress_rgb"]."\" \"#000000\" vid))\n");

fprintf($fd, "\n");
fprintf($fd, "(dump-video vid (/ 1.0 $fps))\n");

fclose($fd);

if ($totalseconds > $doc["maxtime"]) {
    do_header("Render");
    do_banner();
    print "Your movie is longer than the maximum allowed ($doc[maxtime] seconds).";
    die();
}

$queuename = "j".$t."_".$pri."_".$key."_".$task.".q";
$queuepath_tmp = "$QUEUE_DIR/_$queuename";
$queuepath = "$QUEUE_DIR/$queuename";

system("rm -rf $PROJECT_DIR/$taskbase*.mp4");
system("rm -rf $QUEUE_DIR/j*_$key_$taskbase*.q");

$fd = fopen($queuepath_tmp, "w");
if (!$fd) {
    print "<h2>Failed to create queue file</h2>";
    die();
}

fwrite($fd, "#!/bin/bash\n\n");
$VSPATH="/var/www/pdf2mp4/src/";
fwrite($fd, "$VSPATH/vidscheme $VSPATH/vidscheme.scheme $projectscheme | avconv -y -r $fps -f image2pipe -vcodec ppm -i - -b:v $bitrate $outputpath");
fclose($fd);

chmod($queuepath_tmp, 0755);
rename($queuepath_tmp, $queuepath);


header("location: queuewatcher.php?key=$key");
?>
