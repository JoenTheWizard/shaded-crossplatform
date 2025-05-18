#version 330 core

//Input variables from your main program
uniform vec2 iResolution;     //Screen resolution
uniform float iTime;          //Current time in seconds
uniform vec2 iMouse;          //Mouse position
uniform sampler1D iAudio;     //Audio data texture
uniform int iAudioSize;       //Size of audio buffer

in vec2 texCoord;            //Texture coordinates from vertex shader
out vec4 FragColor;          //Output color

//Function to get normalized audio sample at specific index
float getAudio(int index) {
    return texture(iAudio, float(index) / float(iAudioSize)).r;
}

//Function to get normalized audio sample at specific position
float getAudioAtPos(float pos) {
    return texture(iAudio, pos).r;
}

void main() {
    //Normalized pixel coordinates (from 0 to 1)
    vec2 uv = texCoord;
    
    //Center coordinates (from -1 to 1)
    vec2 p = (2.0 * uv - 1.0);
    p.x *= iResolution.x / iResolution.y;  // Correct for aspect ratio
    
    //Initialize color
    vec3 color = vec3(0.0);
    
    //Background color (dark blue)
    vec3 bgColor = vec3(0.0, 0.0, 0.1);
    
    //Bar visualization
    if (uv.y < 0.7) {  //Only show bars in bottom 70% of screen
        //Calculate which bar we're rendering
        float barWidth = 1.0 / float(iAudioSize);
        int barIndex = int(uv.x / barWidth);
        
        //Get audio amplitude for this bar
        float amplitude = abs(getAudio(barIndex));
        
        //If we're within the bar height
        if (uv.y < amplitude * 0.7) {
            //Create color based on frequency (position) and amplitude
            float hue = uv.x;
            float sat = 0.8;
            float val = 0.8 + amplitude * 0.2;
            
            //HSV to RGB conversion
            vec3 rgb;
            float h = hue * 6.0;
            float f = fract(h);
            float p = val * (1.0 - sat);
            float q = val * (1.0 - sat * f);
            float t = val * (1.0 - sat * (1.0 - f));
            
            if (h < 1.0) rgb = vec3(val, t, p);
            else if (h < 2.0) rgb = vec3(q, val, p);
            else if (h < 3.0) rgb = vec3(p, val, t);
            else if (h < 4.0) rgb = vec3(p, q, val);
            else if (h < 5.0) rgb = vec3(t, p, val);
            else rgb = vec3(val, p, q);
            
            color = rgb;
        } else {
            color = bgColor;
        }
    } else {
        //Waveform visualization in top 30% of screen
        float waveY = 0.85;  // Center of waveform
        float waveHeight = 0.1;  // Height of waveform
        
        //Sample audio at position corresponding to x coordinate
        float audioSample = getAudioAtPos(uv.x);
        
        //Calculate distance to waveform
        float dist = abs(uv.y - (waveY + audioSample * waveHeight));
        
        //If close to waveform, show a line
        if (dist < 0.003) {
            color = vec3(0.0, 1.0, 0.5);  //Green waveform
        } else {
            color = bgColor;
        }
    }
    
    //Apply subtle pulsing effect based on the average audio amplitude
    float avgAmplitude = 0.0;
    for (int i = 0; i < iAudioSize; i += 10) {  //Sample every 10th value for performance
        avgAmplitude += abs(getAudio(i));
    }
    avgAmplitude /= (iAudioSize / 10.0);
    
    //Add subtle pulse to the entire scene
    color += vec3(0.05) * avgAmplitude * 2.0;
    
    //Add time-based color variation
    color += 0.05 * vec3(sin(iTime * 0.5), sin(iTime * 0.3), sin(iTime * 0.7));
    
    //Output final color
    FragColor = vec4(color, 1.0);
}
