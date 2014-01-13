import java.io.*;
import java.util.*;

public class GenWorkload
{
    public static void main(String[] args) throws IOException
    {
	if (args.length < 2)
	{
	    System.out.println("Please specify the workload file!");
	    System.exit(1);
	}

	BufferedReader br = new BufferedReader(new FileReader(args[0]));
	BufferedWriter bw = new BufferedWriter(new FileWriter("workload_" + args[0]));
	int numCorePerNode = Integer.parseInt(args[1]);
	String str = br.readLine();
	System.out.println(str);
	while (str != null)
	{
	    if (str.charAt(0) == ';')
	    {
		str = br.readLine();
		continue;
	    }
	    else
	    {
		String[] strLine = str.split(" ");
		int count = 0, length = 0, numNode = 0;
		for (int i = 0; i < strLine.length; i++)
		{
		    if (!strLine[i].equals(""))
		    {
			count++;
			if (count == 4)
			{
			    length = Integer.parseInt(strLine[i]); 
			}
			if (count == 5)
			{
			    numNode = Integer.parseInt(strLine[i]) / numCorePerNode;
			    break;
			}
		    }
		}
		bw.write("srun -N" + numNode + " /bin/sleep " + length + "\r\n");
		str = br.readLine();
	    }
	}
	bw.flush();
	bw.close();
    }
}
