<?php

include("common.php");

$key = $_REQUEST["key"];

if (strlen($key) != 40 || !preg_match("/^[a-z0-9]*$/", $key)) {
   print "Invalid key";
   die();
}

$PROJECT_DIR = $PROJECTS_PATH."/".$key;
$PROJECT_JSON = $PROJECT_DIR."/"."project.json";
$PROJECT_URL = $PROJECTS_URL."/".$key;

$fd = fopen($PROJECT_DIR."/json.txt", "w");
if (!$fd) {
print "\"Failed to write data\"";
die();
}

fputs($fd, $_REQUEST["data"]);
fclose($fd);
print "\"OK\"";
die();
?>
"Unknown Error"
