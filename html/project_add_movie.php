<?php

include("common.php");

do_header("Project editor");
do_banner();

validate_key($key);

?>

<H2>Movie Upload</h2>

<P>Select your MP4 file and click upload. Note that the	maximum	movie size is 100MB and/or 1000 frames, and that any audio data will be discarded. 

<form enctype="multipart/form-data" method=post action=project_add_movie_submit.php>
<input type=hidden name=key value="<?php print $key ?>">
<input type=hidden name=idx value="<?php print $_REQUEST["idx"] ?>">
Movie: <input type=file size=50 name=moviefile><br>
<input type=hidden name=playfps value=30>
<input type=hidden name=mode value="hold">

<br>
    <input type=image src=upload.png name=submit>

</form>

<?php do_footer(); ?>
