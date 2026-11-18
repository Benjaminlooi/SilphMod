package dev.silphmod.nativecheck;

import android.os.SystemClock;
import org.json.*;
import javax.net.ssl.HttpsURLConnection;
import java.io.*;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.time.*;
import java.time.format.DateTimeFormatter;
import java.util.*;
import java.util.regex.*;

final class HuntingFeed {
    static final String URL="https://moonani.com/PokeList/ajax.php?page=pokemon&action=load&iv=100&pvp=0&start=0&length=500";
    private static final Pattern COORDS=Pattern.compile("data-clipboard-text\\s*=\\s*[\"']([^\"']+)[\"']",Pattern.CASE_INSENSITIVE);
    private static final DateTimeFormatter TIME=DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss",Locale.ROOT);
    private static final ZoneOffset ZONE=ZoneOffset.ofHours(7);
    static final class Candidate {
        final int species,cp,level;final double latitude,longitude;final long expires;final String key,name;
        Candidate(int species,int cp,int level,double latitude,double longitude,long expires,String name){this.species=species;this.cp=cp;this.level=level;this.latitude=latitude;this.longitude=longitude;this.expires=expires;this.name=name;key=species+":"+String.format(Locale.ROOT,"%.6f:%.6f",latitude,longitude)+":"+expires;}
    }
    static final class Result {
        final List<Candidate> candidates;final long utc,received,requestMs;final int rows,invalid,expired,shinyUnavailable;
        Result(List<Candidate> candidates,long utc,long received,long requestMs,int rows,int invalid,int expired,int shinyUnavailable){this.candidates=Collections.unmodifiableList(candidates);this.utc=utc;this.received=received;this.requestMs=requestMs;this.rows=rows;this.invalid=invalid;this.expired=expired;this.shinyUnavailable=shinyUnavailable;}
    }
    static Result fetch()throws Exception{
        long begin=SystemClock.elapsedRealtime();HttpsURLConnection connection=(HttpsURLConnection)new URL(URL).openConnection();
        try{connection.setConnectTimeout(10000);connection.setReadTimeout(15000);connection.setInstanceFollowRedirects(false);connection.setRequestProperty("User-Agent","SilphMod/"+BuildConfig.VERSION_NAME);connection.setRequestProperty("Accept","application/json");
            int status=connection.getResponseCode();if(status!=200)throw new IOException("Candidate source HTTP "+status);
            long received=SystemClock.elapsedRealtime(),utc=connection.getHeaderFieldDate("Date",-1);if(utc<0)throw new IOException("Candidate source time unavailable");
            long age=0;try{age=Long.parseLong(connection.getHeaderField("Age"));}catch(Exception ignored){}if(age>=0&&age<=3600)utc+=age*1000;
            byte[] bytes;try(InputStream input=connection.getInputStream();ByteArrayOutputStream output=new ByteArrayOutputStream()){byte[] b=new byte[8192];int n;while((n=input.read(b))!=-1){if(output.size()+n>4194304)throw new IOException("Candidate source response too large");output.write(b,0,n);}bytes=output.toByteArray();}
            JSONObject document=new JSONObject(new String(bytes,StandardCharsets.UTF_8));JSONArray data=document.getJSONArray("data");if(data.length()>3000)throw new IOException("Candidate source row count invalid");
            LinkedHashMap<String,Candidate> out=new LinkedHashMap<>();int invalid=0,expired=0,shinyUnavailable=0;
            for(int i=0;i<data.length();i++)try{JSONObject row=data.getJSONObject(i);if(!shinyAvailable(row)){shinyUnavailable++;continue;}Candidate candidate=parse(row);if(candidate.expires<=utc+3000){expired++;continue;}if(candidate.expires>utc+3600000){invalid++;continue;}out.putIfAbsent(candidate.key,candidate);}catch(Exception error){invalid++;}
            return new Result(new ArrayList<>(out.values()),utc,received,SystemClock.elapsedRealtime()-begin,data.length(),invalid,expired,shinyUnavailable);
        }finally{connection.disconnect();}
    }
    static Candidate parse(JSONObject row)throws Exception{
        if(integer(row,"Attack")!=15||integer(row,"Defense")!=15||integer(row,"HP")!=15)throw new IllegalArgumentException("Candidate IV mismatch");
        int species=integer(row,"Number"),cp=integer(row,"CP"),level=integer(row,"Level");if(species<1||species>=2048||cp<1||cp>20000||level<1||level>55)throw new IllegalArgumentException("Invalid candidate fields");
        Matcher m=COORDS.matcher(row.getString("Coords"));String value=m.find()?m.group(1):plain(row.getString("Coords"));double[] coords=dev.rootcatch.ugphone.KecleonFeed.parseCoordinates(value);
        long expires=LocalDateTime.parse(plain(row.getString("End Time")),TIME).toInstant(ZONE).toEpochMilli();String name=plain(row.optString("Name","#"+species));if(name.length()>96)name=name.substring(0,96);
        return new Candidate(species,cp,level,coords[0],coords[1],expires,name);
    }
    static boolean shinyAvailable(JSONObject row){return !plain(row.optString("Shiny","")).equalsIgnoreCase("No");}
    private static int integer(JSONObject row,String name)throws JSONException{return Integer.parseInt(plain(row.getString(name)));}
    private static String plain(String value){return value.replaceAll("<[^>]*>","").replace("&nbsp;"," ").replace("&amp;","&").trim();}
    private HuntingFeed(){ }
}
