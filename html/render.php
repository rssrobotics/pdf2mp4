<?php

include("common.php");

$key = $_REQUEST["key"];

if (strlen($key) != 40 || !preg_match("/^[a-z0-9]*$/", $key)) {
    print "<h2>Sorry, invalid key.</h2>";
    do_footer();
    die();
}

$PROJECT_DIR = $PROJECTS_PATH."/".$key;
$PROJECT_JSON = $PROJECT_DIR."/"."json.txt";
$PROJECT_URL = $PROJECTS_URL."/".$key;

$fd = fopen($PROJECT_JSON, "r");
if (!$fd) {
    print "Failed to read json.";
  // new document
    die();
}

$json = fgets($fd);
fclose($fd);

$doc = json_decode($json, 1);

if ($_REQUEST["preview"]==1) {
    $fps = 1;
    $h = 320;
    $slideselector = "thumb";
    $bitrate = "2M";
    $task = "preview";
    $outputpath = $PROJECT_DIR."/$task.mp4";
    $outputurl = $PROJECT_URL."/$task.mp4";
    $pri = 0;
} else {
    $fps = 30;
    $h = 1080;
    $slideselector = "slide";
    $bitrate = "8M";
    $task = "final";
    $outputpath = $PROJECT_DIR."/$task.mp4";
    $outputurl = $PROJECT_URL."/$task.mp4";
    $timebonus = 0;
    $pri = 1;
}

chmod($PROJECT_DIR, 0777);
unlink($outputpath);

$w = intval(1920*$h/1080);

/////////////////////////////////////////
// write the output
$fd = fopen($PROJECT_DIR."/project.scheme", "w");

for ($i = 0; $i < count($doc["slides"]); $i++) {
    $slide = $doc["slides"][$i];

    fprintf($fd, "(define slide_$i \n");
    fprintf($fd, "  (image-source-matte $w $h \"#000000\"\n");
    fprintf($fd, "    (image-source-create-from-image ".intval($fps*$slide["seconds"])."\n");
    fprintf($fd, "      (image-create-from-file \"$PROJECT_DIR/".$slide[$slideselector]."\"))))\n");

    if ($slide["progress"])
        fprintf($fd, "(set! slide_$i (image-source-progress-bar -1 4 255 0 0 0 0 0 slide_$i))\n");

    fprintf($fd, "\n");
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

/*
$fd = fopen($PROJECT_DIR."/project.scheme", "r");
while (($s = fgets($fd)) != NULL)
    print $s;
    fclose($fd);
*/
$tmp = tempnam("/tmp", "pdf2mp4");

$QUEUEDIR = "/tmp/queueworker/";

$queuename = "j".time()."_".$pri."_".$key."_".$task.".q";
$queuepath_tmp = "$QUEUEDIR/_$queuename";
$queuepath = "$QUEUEDIR/$queuename";

$fd = fopen($queuepath_tmp, "w");
if (!$fd) {
    print "<h2>Failed to create queue file</h2>";
    die();
}

fwrite($fd, "#!/bin/bash\n\n");
$VSPATH="/var/www/pdf2mp4/src/";
fwrite($fd, "$VSPATH/vidscheme $VSPATH/vidscheme.scheme $PROJECT_DIR/project.scheme | avconv -y -r $fps -f image2pipe -vcodec ppm -i - -b $bitrate $outputpath");
fclose($fd);

chmod($queuepath_tmp, 0755);
rename($queuepath_tmp, $queuepath);


header("location: queuewatcher.php?key=$key");
?>
