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
    $fps = 1;
    $h = 320;
    $slideselector = "thumb";
    $bitrate = "2M";
    $taskbase = "preview";
    $pri = 0;
} else {
    $fps = 30;
    $h = 1080;
    $slideselector = "slide";
    $bitrate = "8M";
    $taskbase = "final";
    $pri = 1;
}

$task = $taskbase."_$t";
$outputpath = $PROJECT_DIR."/$task.mp4";
$outputurl = $PROJECT_URL."/$task.mp4";
$projectscheme = $PROJECT_DIR."/project_$task.scheme";

chmod($PROJECT_DIR, 0777);
unlink($outputpath);

$w = intval(1920*$h/1080);

/////////////////////////////////////////
// write the output
$fd = fopen($projectscheme, "w");

$totalseconds = 0;

for ($i = 0; $i < count($doc["slides"]); $i++) {
    $slide = $doc["slides"][$i];

    $slidepath = $PROJECT_DIR."/".$slide[$slideselector];

    fprintf($fd, "(define slide_$i \n");
    fprintf($fd, "  (image-source-matte $w $h \"#000000\"\n");
    fprintf($fd, "    (image-source-create-from-image-cache ".intval($fps*$slide["seconds"])." \"$slidepath\")))\n");
//    fprintf($fd, "    (image-source-create-from-image ".intval($fps*$slide["seconds"])."\n");
//    fprintf($fd, "      (image-create-from-file \"$PROJECT_DIR/".$slide[$slideselector]."\"))))\n");

    if ($slide["progress"])
        fprintf($fd, "(set! slide_$i (image-source-progress-bar -1 4 255 0 0 0 0 0 slide_$i))\n");

    fprintf($fd, "\n");

    $totalseconds += $slide["seconds"];
}

fprintf($fd, "\n");

fprintf($fd, "(define vid (crossfade-all ".intval(0.75*$fps)." (list\n");

for ($i = 0; $i < count($doc["slides"]); $i++) {
    fprintf($fd, "                               slide_$i\n");
}

fprintf($fd, "                               (image-source-create-from-image ".intval(2*$fps)." (image-create $w $h \"#000000\")))))\n");
fprintf($fd, "\n");
//fprintf($fd, "(set! vid (image-source-decimate-frames ".intval($fps/2)." tmp))\n");
fprintf($fd, "\n");
if ($doc["progress"])
    fprintf($fd, "(set! vid (image-source-progress-bar 0 4 255 0 0 0 0 0 vid))\n");

fprintf($fd, "\n");
fprintf($fd, "(dump-video vid)\n");

fclose($fd);

if ($totalseconds > $MAX_MOVIE_SECONDS) {
    do_header("Render");
    do_banner();
    print "Your movie is longer than the maximum allowed ($MAX_MOVIE_SECONDS seconds).";
    die();
}

$QUEUEDIR = "/tmp/queueworker/";

$queuename = "j".$t."_".$pri."_".$key."_".$task.".q";
$queuepath_tmp = "$QUEUEDIR/_$queuename";
$queuepath = "$QUEUEDIR/$queuename";

system("rm -rf $PROJECT_DIR/$taskbase*.mp4");
system("rm -rf $QUEUEDIR/j*_$key_$taskbase*.q");

$fd = fopen($queuepath_tmp, "w");
if (!$fd) {
    print "<h2>Failed to create queue file</h2>";
    die();
}

fwrite($fd, "#!/bin/bash\n\n");
$VSPATH="/var/www/pdf2mp4/src/";
fwrite($fd, "$VSPATH/vidscheme $VSPATH/vidscheme.scheme $projectscheme | avconv -y -r $fps -f image2pipe -vcodec ppm -i - -pre ultrafast -b $bitrate $outputpath");
fclose($fd);

chmod($queuepath_tmp, 0755);
rename($queuepath_tmp, $queuepath);


header("location: queuewatcher.php?key=$key");
?>
