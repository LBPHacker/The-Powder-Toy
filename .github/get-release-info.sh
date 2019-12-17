echo ::set-output name=upload_url::`cat release_info.txt | cut -d ' ' -f 1`
echo ::set-output       name=type::`cat release_info.txt | cut -d ' ' -f 2`
echo ::set-output       name=name::`cat release_info.txt | cut -d ' ' -f 3`
