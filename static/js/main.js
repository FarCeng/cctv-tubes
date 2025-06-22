document.addEventListener('DOMContentLoaded', () => {

    //Definisi Elemen
    const controls = {
        tilt: {
            slider: document.getElementById('tilt'),
            number: document.getElementById('tilt-number')
        },
        pan: {
            slider: document.getElementById('pan'),
            number: document.getElementById('pan-number')
        },
        cahaya: {
            slider: document.getElementById('cahaya'),
            number: document.getElementById('cahaya-number')
        }
    };
    
    const statusText = document.getElementById('status-text');

    //Logika Sinkronisasi Slider dan Input Angka
    function setupControlSync(controlName) {
        const { slider, number } = controls[controlName];
        const min = parseInt(slider.min, 10);
        const max = parseInt(slider.max, 10);

        //Sinkronisasi dari Slider ke Input Angka
        slider.addEventListener('input', () => {
            number.value = slider.value;
        });

        //Sinkronisasi dari Input Angka ke Slider dengan Validasi
        number.addEventListener('input', () => {
            let value = parseInt(number.value, 10);
            // Jika input bukan angka, jangan lakukan apa-apa
            if (isNaN(value)) {
                return;
            }
            if (value > max) {
                value = max;
                number.value = max;
            }
            if (value < min) {
                value = min;
                number.value = min;
            }
            
            slider.value = value;
        });

        // Handle jika input angka dikosongkan
        number.addEventListener('blur', () => {
             if (number.value === '') {
                number.value = slider.value;
            }
        });
    }

    setupControlSync('tilt');
    setupControlSync('pan');
    setupControlSync('cahaya');

    // Logika Tombol
    // Tombol Kirim Perintah
    document.getElementById('send-controls').addEventListener('click', () => {
        const controlsData = {
            tilt: controls.tilt.slider.value,
            pan: controls.pan.slider.value,
            cahaya: controls.cahaya.slider.value
        };

        statusText.textContent = 'Mengirim perintah...';

        fetch('/update_controls', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(controlsData)
        })
        .then(response => response.json())
        .then(data => {
            statusText.textContent = data.status === 'success' ? 'Perintah berhasil dikirim!' : 'Gagal mengirim perintah.';
            setTimeout(() => statusText.textContent = '', 3000);
        })
        .catch(error => {
            console.error('Error:', error);
            statusText.textContent = 'Error koneksi.';
        });
    });

    // Logika Stream dan Capture
    const streamBtn = document.getElementById('stream-btn');
    const videoElement = document.getElementById('video-stream');
    const placeholder = document.querySelector('.camera-view .placeholder');
    let isStreaming = false;

    streamBtn.addEventListener('click', () => {
        fetch('/toggle_stream', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            isStreaming = data.streaming;
            if (isStreaming) {
                videoElement.src = `/video_feed?t=${new Date().getTime()}`;
                videoElement.style.display = 'block';
                placeholder.style.display = 'none';
                streamBtn.textContent = 'Stop Stream';
                streamBtn.style.backgroundColor = '#dc3545';
            } else {
                videoElement.src = '';
                videoElement.style.display = 'none';
                placeholder.style.display = 'block';
                streamBtn.textContent = 'Mulai Stream';
                streamBtn.style.backgroundColor = '#333';
            }
        });
    });

    document.getElementById('capture-btn').addEventListener('click', () => {
        statusText.textContent = 'Mengirim perintah capture...';
        fetch('/trigger_capture', { method: 'POST' })
        .then(response => response.json())
        .then(data => {
            if(data.status === 'capture signal sent') {
                statusText.textContent = 'Capture terkirim! Menunggu gambar...';
                
                if (!isStreaming) {
                    setTimeout(() => {
                        videoElement.src = `/video_feed?t=${new Date().getTime()}`;
                        videoElement.style.display = 'block';
                        placeholder.style.display = 'none';
                        statusText.textContent = 'Gambar capture diterima!';
                        setTimeout(() => statusText.textContent = '', 3000);
                    }, 2000); 
                }
            }
        }).catch(() => {
            statusText.textContent = 'Gagal capture!';
            setTimeout(() => statusText.textContent = '', 3000);
        });
    });
});