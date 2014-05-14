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
<span onclick='move_previous()'><img src=move_previous.png></span>
	<span style="width:20px; display:inline-block"></span>
	<img id=prev_button src=prev_button.png><span id=current_slide_label></span>
	<img id=next_button src=next_button.png>

	<span style="width:20px; display:inline-block"></span>
<span onclick='move_next()'><img src=move_next.png></span>

	<img id=delete_button src=delete.png>
	<img id=delete_button src=add_movie.png>

<br><br>
	Slide duration: <input type=text id=slide_seconds size=5> seconds (used <span id=total_seconds>0</span> of <span id=maxtime_span></span> allowed)<br><br>

	Render slide progress bar: <input id=slide_progress_bar type=checkbox>
	</center>
      </div>
    </div>
    </center>

    <td valign=top>
    <div id=status_div>
Name: <br><input type=text id=project_name style="width: 100%"><br><br>
      <span id=status_label></span><br>
      Render overall progress bar: <input id=global_progress_bar type=checkbox><br><br>

<center>
<a href="render.php?key=<?php print $key?>&preview=1"><img src=render_preview.png></a><br>
<font size=-1><span id=preview_description></span></font></center><br>

<center>
    <a href="render.php?key=<?php print $key?>&preview=0"><img src=render_final.png></a><br>
    <font size=-1><span id=final_description></span> (slow!)</font></center><br>

    <center>
<a href="queuewatcher.php?key=<?php print $key?>">See render queue/results</a><br><br>
</center>

    </div>
  </tr>
</table>

</center>

<script>

var project_url = "<?php echo $PROJECT_URL ?>";
var key = "<?php echo $key ?>";

xmlhttp_post("project_read.php", "key="+key,
             function(xml, url) {
                 try {
                     doc_restore(xml.responseText);
                     update_descriptions();
                 } catch (ex) {
                     alert(xml.responseText);
                 }
             });


</script>


<?php
do_footer();
?>
