<?php

include("common.php");

do_header("Project render");
do_banner();

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

$fd = fopen($PROJECT_DIR."/project.scheme", "w");

$fps = 30;

$h = 320;
$w = intval(1920*$h/1080);

for ($i = 0; $i < count($doc["slides"]); $i++) {
    $slide = $doc["slides"][$i];

    fprintf($fd, "(define slide_$i \n");
    fprintf($fd, "  (image-source-matte $w $h \"#000000\"\n");
    fprintf($fd, "    (image-source-create-from-image ".intval($fps*$slide["seconds"])."\n");
    fprintf($fd, "      (image-create-from-file \"$PROJECT_DIR/".$slide["thumb"]."\"))))\n");

    if ($slide["progress"])
        fprintf($fd, "(set! slide_$i (image-source-progress-bar 4 4 255 0 0 0 0 0 slide_$i))\n");

    fprintf($fd, "\n");
}

fprintf($fd, "\n");

fprintf($fd, "(define vid (crossfade-all 20 (list\n");

for ($i = 0; $i < count($doc["slides"]); $i++) {
    fprintf($fd, "                               slide_$i\n");
}

fprintf($fd, "                               (image-source-create-from-image 200 (image-create $w $h \"#000000\")))))\n");
fprintf($fd, "\n");
fprintf($fd, "(set! vid (image-source-decimate-frames ".intval($fps/2)." tmp))\n");
fprintf($fd, "\n");
if ($doc["progress"])
    fprintf($fd, "(set! vid (image-source-progress-bar 0 4 255 0 0 0 0 0 vid))\n");

fprintf($fd, "\n");
fprintf($fd, "(dump-video vid)\n");

fclose($fd);

$fd = fopen($PROJECT_DIR."/project.scheme", "r");
while (($s = fgets($fd)) != NULL)
    print $s;
fclose($fd);


?>
