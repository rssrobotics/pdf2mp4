<?php

include("common.php");

validate_key($key);

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
