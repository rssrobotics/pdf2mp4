<?php

include("common.php");

do_header("Project editor");
do_banner();

validate_key($key);

?>

<!-- the primary slide display area with the larger thumbnail and arrow controls -->
<center>
<table border=0 width=100%>
  <tr>
    <td><!-- the tiny preview slides will go in here -->
      <center>
    <div id=slide_nav>
    </div>
    </center>

    <td valign=top>
      <center>
    <div id=thumb_area>
      <img id=current_slide class=thumb><br>
      <center>
      <div id=controls_area>
	<img id=prev_button src=prev_button.png><span id=current_slide_label></span>
	<img id=next_button src=next_button.png><br><br>
	Seconds: <input type=text id=slide_seconds size=5><br><br>
	Show progress bar: <input id=slide_progress_bar type=checkbox>
	</center>
      </div>
    </div>
    </center>

    <td valign=top>
    <div id=status_div>
      Project name: <input type=text id=project_name><br><br>
      Status: <span id=status_label></span><br><br>
      Total seconds: <span id=total_seconds>0</span><br>
(RSS 2014 maximum <?php print $MAX_MOVIE_SECONDS ?>)<br><br>
      Show overall progress bar: <input id=global_progress_bar type=checkbox><br><br>
<a href="render.php?key=<?php print $key?>&preview=1">Render preview video</a><br>
<font size=-1>320p @ 1fps</font><br><br>

    <a href="render.php?key=<?php print $key?>&preview=0">Render final video</a><br>
    <font size=-1>1080p @ 30fps -- slow!</font><br><br>
<a href="queuewatcher.php?key=<?php print $key?>">See render queue/results</a><br><br>
    </div>
  </tr>
</table>

</center>

<script>
var project_url = "<?php echo $PROJECT_URL ?>";
var key = "<?php echo $key ?>";

<?php
$fd = fopen($PROJECT_JSON, "r");
if (!$fd) {
  // new document
  print "doc_create(8);\n";
} else {
 $s = fgets($fd);
 print "doc_restore('$s');\n";
}

?>
</script>


<?php
do_footer();
?>
