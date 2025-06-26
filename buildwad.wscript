entry main(args) {
    wad_content_path = args[0];
    check (err) {
        wad = wadfilecreate("portdata.wad");
        wadsettype(wad, "pwad");
        each (fpath: filelist(wad_content_path, false, null)) {
            file = fisopen(fpath);
            fname = substr(fpath, strlastindex(fpath, "/")+1, null);
            wadadd(wad, fname, file, null);
        }
    }
    if (err != null) {
        println("error: " + err);
        return 1;
    }
    close(wad);
}
