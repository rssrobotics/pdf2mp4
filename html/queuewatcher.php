<?php

include("common.php");

do_header("Project render");
do_banner();

validate_key($key);

$files = array();
$dir = opendir($QUEUE_DIR);
if (!$dir) {
    print "failed to open $QUEUE_DIR";
    die();
}

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

    if (preg_match('/^j([0-9]*)_([0-9]*)_([0-9a-fA-F]*)_([a-z0-9_]*).([cpq].*)$/', $file, $matches)) {

        $jtime = $matches[1];
        $jpri = $matches[2];
        $jkey = $matches[3];
        $jtask = $matches[4]; // e.g. preview/final
        $jstatus = $matches[5];

        // don't show completed tasks from other users
        if (str_starts_with($jstatus, "c") && $jkey != $key)
            continue;

        $moviepath = $PROJECT_DIR."/".$jtask.".mp4";
        $movieurl = $PROJECT_URL."/".$jtask.".mp4";

        // don't show old completed tasks whose output has since been removed
        if (str_starts_with($jstatus, "c") && !file_exists($moviepath))
            continue;

//        print "<tr><td>$jpri / $jtime";
        print "<tr><td>$jtime";

        if ($jkey==$key)
            print "<td>Your project ($jtask)<br>";
        else
            print "<td>Another project ($jtask)<br>";

        print "<td align=center>";
        $elapsed = time() - intval($jtime);

        $ppath = $QUEUE_DIR."/j".$jtime."_".$jpri."_".$jkey."_".$jtask.".p";

        $processtime = time() - filemtime($ppath);

        if (str_starts_with($jstatus, "p")) {
            print "Processing...<br>";
            print "<font size=-1>(queue+process time: $elapsed seconds)</font><br>";
//            print "<font size=-1>(queue time: ".($elapsed-$processtime)." seconds)</font><br>";
//            print "<font size=-1>(process time: $processtime seconds)</font><br>";
            $opath = $QUEUE_DIR."/j".$jtime."_".$jpri."_".$jkey."_".$jtask.".out";
            $ofd = fopen($opath, "r");
            $ofd_size = filesize($opath);
            fseek($ofd, max(0, $ofd_size - 1024));

            $stat = "";
            while (($s = fgets($ofd)) != NULL) {
                if (preg_match('/QQframe ([0-9]+) \\/ ([0-9]+)QQ/', $s, $matches)) {
		    $eta = (intval($matches[2]) - intval($matches[1])) * $processtime / intval($matches[1]);
                    $stat = "<font size=-1>frame $matches[1] / $matches[2]<br>Time remaining: ".intval($eta)." seconds</font><br>";
                    break;
                }
            }
            print $stat;

            fclose($ofd);

        } else if (str_starts_with($jstatus, "q")) {
            print "Queued...<br>";
            print "<font size=-1>(queue time: $elapsed seconds)</font><br>";
        } else if (str_starts_with($jstatus, "c")) {
            if ($key == $jkey) {

                print "<a href=$movieurl><img src=download.png></a><br>";
                $dt = filemtime($moviepath) - $jtime;
                print "<font size=-1>(queue+process time: $dt seconds)</font>";
            }
        }
    }
}
print "</table>\n";
print "<br><I>Last refreshed ".date("r")."</I>";
print "</center>";
?>
<script>
setTimeout(function() { document.location = "queuewatcher.php?key=<?php print $key ?>"; },
           5000);
</script>

<?php
do_footer();
?>
