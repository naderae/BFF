from django.db import models
from django.contrib.auth.models import User
from groups.models import Group

# Create your models here.
class Post(models.Model):
    title = models.CharField(max_length=255, unique=True)
    body = models.TextField()
    likes_total = models.IntegerField(default=0)
    likers = models.ManyToManyField(User, related_name='likers')
    pub_date = models.DateTimeField()
    author = models.ForeignKey(User, on_delete=models.CASCADE, related_name= 'author')
    group = models.ForeignKey(Group, on_delete=models.CASCADE)


    def __str__(self):
        return self.title

    def summary(self):
        # return the 1st 100 chars
        return self.body[:100]

    def pub_date_pretty(self):
        # strftime is how to break down time
        return self.pub_date.strftime('%b %e %Y')
