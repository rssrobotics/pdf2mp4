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
$QUEUE_DIR = "/tmp/queueworker/";

$files = array();
$dir = opendir($QUEUE_DIR);
while (($file = readdir($dir)) !== false) {
    if (preg_match('/^j([0-9]*)_([0-9]*)_([0-9a-fA-F]*)_([a-z]*).(.*)$/', $file, $matches)) {
        $files[] = $file;

    }
}

sort($files);

print "<center>";
print "<table border=1><tr><td>Priority (lower is higher)<td>Job<td>Status\n";

for ($i = 0; $i < count($files); $i++) {
    $file = $files[$i];

    if (preg_match('/^j([0-9]*)_([0-9]*)_([0-9a-fA-F]*)_([a-z]*).([cpq].*)$/', $file, $matches)) {

        $jtime = $matches[1];
        $jpri = $matches[2];
        $jkey = $matches[3];
        $jtask = $matches[4]; // e.g. preview/final
        $jstatus = $matches[5];

        // don't show completed tasks from other users
        if (str_starts_with($jstatus, "c") && $jkey != $key)
            continue;

        print "<tr><td>$jpri / $jtime";

        if ($jkey==$key)
            print "<td>Your project ($jtask)<br>";
        else
            print "<td>Another project ($jtask)<br>";

        print "<td>";
        if (str_starts_with($jstatus, "p")) {
            $elapsed = time() - intval($jtime);
            print "Processing... ($elapsed seconds elapsed)";
        } else if (str_starts_with($jstatus, "q")) {
            print "Queued";
        } else if (str_starts_with($jstatus, "c")) {
            print "Completed ";
            if ($jkey==$key)
                print "<a href=$PROJECT_URL/".$jtask.".mp4>".$jtask.".mp4</a>";
        }
    }
}
print "</table>";
print "<I>Last refreshed ".date("r")."</I>";
print "</center>";
?>
<script>
setTimeout(function() { document.location = "queuewatcher.php?key=<?php print $key ?>"; },
           5000);
</script>

<?php
do_footer();
?>
