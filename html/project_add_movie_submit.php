<?php

include("common.php");

do_header("Project editor");
do_banner();

validate_key($key);

$MOVIE_NAME = "video_".sha1(rand().rand().rand().rand());

$MOVIE_DIR = $PROJECT_DIR."/".$MOVIE_NAME;
mkdir($MOVIE_DIR);

print "<h2>Movie import status</h2>";

print "File uploaded succeeded....<br><br>\n";

print "Detecting frame rate...";

if (1) {
    $fd = popen("avprobe ".$_FILES['moviefile']['tmp_name']." 2>&1", "r");

    if (!$fd) {
        print "failed to open downloaded file";
        die();
    }

    $fps = -1;

    while (!feof($fd)) {
        $s = fgets($fd);
        if (preg_match('/, ([0-9]+[.]?[9-9]*) fps,/', $s, $matches)) {
            $fps = $matches[1];
        }
    }
    fclose($fd);
    if ($fps > 0) {
        print "... detected $fps fps";
    } else {
        print "... hmmm, I couldn't process this video. Is it an MP4?";
        die();
    }
}

print "<h3>Extracting frames... (this can take minutes)</h3><br><br>\n";

myflush();

system("avconv -i ".$_FILES['moviefile']['tmp_name']." -f image2 $MOVIE_DIR/frame%08d.png");

$frames = array();
$dir = opendir($MOVIE_DIR);
while (($file = readdir($dir)) !== false) {
  if (str_starts_with($file, "frame"))
     $frames[] = $file;
}

sort($frames);

if (count($frames) == 0) {
    print "<h2>I couldn't decode any frames. Is this an MP4?</h2>";
    die();
}

closedir($dir);
print "Extracted ".count($frames)." frames.";

?>

<script>
xmlhttp_post("project_read.php", "key=<?php print $key ?>",
             function(xml, url) {
                 try {
                     doc = JSON.parse(xml.responseText);

                     var newslide = new Object();
                     newslide['type'] = "movie";
                     newslide['dir'] = "<?php print $MOVIE_NAME ?>";
                     newslide['thumb'] = "<?php print $MOVIE_NAME."/".$frames[count($frames)/2] ?>";
                     newslide['progress'] = 1;
                     newslide['playfps'] = <?php print $fps ?>;
                     newslide['nframes'] = <?php print count($frames) ?>;
                     newslide['seconds'] = newslide['nframes'] / newslide['playfps'];
                     newslide['mode'] = "<?php print $_REQUEST["mode"] ?>";
                     doc.slides.splice(<?php print $_REQUEST["idx"] ?>, 0, newslide);

                     doc_save_real();

                 } catch (ex) {
                     alert("error: "+xml.statusText);
                 }
             });
</script>

<center>
<h2><a href=project.php?key=<?php print $key?>>Great, take me back to the project</a></h2>
</center>

<?php
do_footer();
?>
