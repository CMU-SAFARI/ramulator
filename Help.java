import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.util.*;
import java.io.PrintWriter;

public class Help{
  public static ArrayList<String> fileList = new ArrayList<String>();
	public static void main(String[] args) {
    if( args.length<1) {
      System.out.println("to get results give arg. 0, to get slowdown.csv give arg. 1.");
      System.exit(0);
    }
    if(args[0].equals("0")) {
      System.out.println(args[0]);
      getResults(0);
      //getResults(1);
    }
    else {
      try{
        PrintWriter pw = new PrintWriter(new File("slowdown2-0L3.csv"));
        pw.write("trace,slowdown,stt,ddr3\n");
        initStatList();
        for (String trace : fileList) {
          String txt = trace.substring(trace.indexOf(".")+1) + ","
          + getSlowdown(trace) + "," + getCPI("./results2-0L3/"
          + trace + "_stt.stats") + "," + getCPI("./results2-0L3/"
          + trace + "_ddr3.stats") +"\n";
          pw.write(txt);
          System.out.println(txt);
        }
        pw.close();
      }
      catch (Exception e) {
        e.printStackTrace();
      }
    }
	}
  public static void initStatList(){
    String traces = "./cputraces/";
    File dir = new File(traces);
    File[] directoryListing = dir.listFiles();
    if (directoryListing != null) {
      for (File child : directoryListing) {
        if(child.getName().indexOf(".gz")<0) {
          fileList.add(child.getName());
        }
      }
    }
  }
  public static double getSlowdown(String traceName){
    String stt_name = "./results2-0L3/" + traceName + "_stt.stats";
    String ddr3_name = "./results2-0L3/" + traceName + "_ddr3.stats";
    double stt = getCPI(stt_name);
    double ddr3 = getCPI(ddr3_name);
    double slowdown = ((stt - ddr3) *100.0 / ddr3);
    return slowdown;
  }
  public static void getResults(int cfg){ //if cfg is 0 stt-mram, 1 ddr3
    String traces = "./cputraces/";
    File dir = new File(traces);
    File[] directoryListing = dir.listFiles();
    if (directoryListing != null) {
      for (File child : directoryListing) {
      //  System.out.println(child.getName());
        if(child.getName().indexOf(".gz")<0) {
          fileList.add(child.getName());

          String config = "";
          String statfile = "";
          if (cfg==0) {
            config = "configs/STTMRAM-config.cfg";
            statfile = "./results2-0L3/" + child.getName() + "_stt.stats";
          }
          else {
            config = "configs/DDR3-config.cfg";
            statfile = "./results2-0L3/" + child.getName() + "_ddr3.stats";
          }
          String mode = "--mode=cpu";
          String tracePath = child.getAbsolutePath();
          try {
            Process process = new ProcessBuilder("./ramulator",config, mode, "--stats", statfile, tracePath).start();
          }
          catch (IOException e) {
            e.printStackTrace();
          }
        }
      }
    }
    else {
      System.out.println("oops");
    }
  }
  public static double getCPI(String filename){
    try {
			File file = new File(filename);
			FileReader fileReader = new FileReader(file);
			BufferedReader bufferedReader = new BufferedReader(fileReader);
			StringBuffer stringBuffer = new StringBuffer();
			String line;
      long cycles = 0;
      long insts = 0;
			while ((line = bufferedReader.readLine()) != null) {
        if (line.indexOf("ramulator.cpu_cycles")>=0) {
          StringTokenizer t = new StringTokenizer(line, " ");
          int i = 0;
          while(t.hasMoreTokens()){
            String token = t.nextToken();
            if(isNumber(token))
              cycles = Integer.parseInt(token);
            i++;
          }
        }
        else if (line.indexOf("ramulator.cpu_instructions_core_0")>=0){
          StringTokenizer t = new StringTokenizer(line, " ");
          int i = 0;
          while(t.hasMoreTokens()){
            String token = t.nextToken();
            if(isNumber(token))
              insts = Integer.parseInt(token);
            i++;
          }
        }
			}
			fileReader.close();
      return cycles / ((double)insts);
		} catch (IOException e) {
			e.printStackTrace();
		}
    return 0;
  }
  public static boolean isNumber(String s) {
    for(int i =0;i<s.length();i++){
      if(!Character.isDigit(s.charAt(i)))
        return false;
    }
    return true;
  }
}
