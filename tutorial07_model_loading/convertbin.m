parfor i =1:200
    fn = sprintf("tmp%d.bin", i);
    f = fopen(fn, "rb");
    height = fread(f, 1, 'int');
    width = fread(f, 1, 'int');
    pix_n_byte = fread(f, 1, 'int');
    data = fread(f, height*width*pix_n_byte, 'uint8');
    data = data/255;
    data = permute(reshape(data, pix_n_byte, width, height), [3,2,1]);
    data = flipud(data(:,:,1:3));
    imwrite(data, sprintf('tmp%d.bmp', i));
    fclose(f);
end
fclose('all');
!ffmpeg -i tmp%d.bmp -c:v libx264 -pix_fmt yuv420p -y out.mp4
!rm *bin tmp*bmp