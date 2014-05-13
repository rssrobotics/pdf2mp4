<?php

include("common.php");

validate_key($key);

$fd = fopen($PROJECT_DIR."/json.txt", "r");
if (!$fd) {
    print "\"ERROR\"";
    die();
}

$s = fgets($fd);
print $s;
fclose($fd);
die();
?>
